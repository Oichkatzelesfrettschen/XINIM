#include "ring3.hpp"
#include "user_backing.hpp"

#include "ring3_internal.hpp"
#include "kutil.hpp"
#include "process.hpp"
#include "signal.hpp"
#include "sched.hpp"
#include "hw_init.hpp"
#include "console.hpp"
#include "ext2_reader.hpp"
#include "socket_i486.hpp"
#include "sysv_ipc.hpp"
#include "lockf.hpp"

#include "bootfs.hpp"
#include "elf32_loader.hpp"
#include "shell.hpp"
#include "xinim/sys/syscalls.h"

namespace xinim::i486::ring3 {

// -- Compiler runtime ------------------------------------------------------

extern "C" void* memset(void* destination, int value, uint32_t size) noexcept {
    auto* output = static_cast<uint8_t*>(destination);
    if (output == nullptr) {
        return nullptr;
    }
    for (uint32_t index = 0U; index < size; ++index) {
        output[index] = static_cast<uint8_t>(value);
    }
    return destination;
}

// -- Global variable definitions -------------------------------------------

alignas(16) GdtEntry g_gdt[7]{};
alignas(16) TssEntry g_tss{};
alignas(16) IdtEntry g_idt[256]{};
alignas(16) uint8_t g_bootstrap_kernel_stack[kKernelStackSize]{};
alignas(16) Process g_processes[kMaxProcesses]{};
static_assert(user_backing::kImageBytes == elf32::kUserAddressSpaceSize);
static_assert(user_backing::kMaximumImages == kMaxProcesses + 1U);
SupervisedService g_supervised_services[kMaxSupervisedServices]{};
xinim::kernel::recovery::RecoveryDag g_service_recovery_dag{};
const xinim::boot::BootInfo* g_boot_info = nullptr;
Process* g_current_process = nullptr;
uint32_t g_next_pid = 1U;
uint32_t g_next_service_id = 1U;
uint64_t g_scheduler_ticks = 0U;
uint32_t g_boot_epoch_seconds = 0U;
uint64_t g_boot_epoch_ticks = 0U;

// -- Local helpers (anonymous namespace) -----------------------------------

namespace {

[[nodiscard]] Process* find_waiting_child(Process* parent,
                                          int32_t requested_pid,
                                          bool include_stopped = false) noexcept {
    if (parent == nullptr) {
        return nullptr;
    }
    for (auto& process : g_processes) {
        if (!process.in_use || process.ppid != parent->pid) {
            continue;
        }
        const bool match = (process.state == ProcessState::Exited) ||
                           (include_stopped && process.state == ProcessState::Stopped);
        if (!match) {
            continue;
        }
        if (requested_pid == kWaitPidAny || static_cast<int32_t>(process.pid) == requested_pid) {
            return &process;
        }
    }
    return nullptr;
}

[[nodiscard]] uint32_t compute_user_stack_limit(Process* process) noexcept {
    if (process == nullptr) {
        return elf32::kUserVirtualBase;
    }
    const uint32_t stack_guard_floor =
        (process->context.esp > kHeapGuardBytes) ? process->context.esp - kHeapGuardBytes : 0U;
    return align_down(stack_guard_floor, kPageSize);
}

[[nodiscard]] uint32_t lowest_mapping_base(Process* process) noexcept {
    uint32_t floor = elf32::kUserVirtualBase + elf32::kUserAddressSpaceSize;
    bool found = false;
    if (process == nullptr) {
        return floor;
    }
    for (const auto& mapping : process->mappings) {
        if (!mapping.in_use) {
            continue;
        }
        if (!found || mapping.address < floor) {
            floor = mapping.address;
            found = true;
        }
    }
    return floor;
}

[[nodiscard]] UserMapping* find_free_mapping_slot(Process* process) noexcept {
    if (process == nullptr) {
        return nullptr;
    }
    for (auto& mapping : process->mappings) {
        if (!mapping.in_use) {
            return &mapping;
        }
    }
    return nullptr;
}

[[nodiscard]] UserMapping* find_mapping(Process* process,
                                        uint32_t address,
                                        uint32_t size) noexcept {
    if (process == nullptr) {
        return nullptr;
    }
    for (auto& mapping : process->mappings) {
        if (mapping.in_use && mapping.address == address && mapping.size == size) {
            return &mapping;
        }
    }
    return nullptr;
}

[[nodiscard]] uint32_t allocate_mapping_base(Process* process, uint32_t size) noexcept {
    if (process == nullptr || size == 0U) {
        return 0U;
    }

    const uint32_t lower_bound = align_up(process->current_break, kPageSize);
    uint32_t cursor = compute_user_stack_limit(process);
    if (cursor <= lower_bound) {
        return 0U;
    }

    while (cursor > lower_bound) {
        const UserMapping* nearest = nullptr;
        for (const auto& mapping : process->mappings) {
            if (!mapping.in_use || mapping.address >= cursor) {
                continue;
            }
            if (nearest == nullptr || mapping.address > nearest->address) {
                nearest = &mapping;
            }
        }

        const uint32_t gap_floor = (nearest == nullptr)
                                       ? lower_bound
                                       : align_up(nearest->address + nearest->size, kPageSize);
        if (cursor > gap_floor && cursor - gap_floor >= size) {
            return align_down(cursor - size, kPageSize);
        }

        if (nearest == nullptr) {
            break;
        }
        cursor = align_down(nearest->address, kPageSize);
    }

    return 0U;
}

template <uint32_t Count>
void initialize_pointer_vector(uint32_t (&dest)[Count], uint32_t value) noexcept {
    for (uint32_t index = 0U; index < Count; ++index) {
        dest[index] = value;
    }
}

[[nodiscard]] bool write_initial_bytes(uint8_t* backing,
                                       uint32_t user_address,
                                       const void* source,
                                       uint32_t size) noexcept {
    if (backing == nullptr || source == nullptr || user_address < elf32::kUserVirtualBase) {
        return false;
    }
    const uint32_t offset = user_address - elf32::kUserVirtualBase;
    if (offset > elf32::kUserAddressSpaceSize ||
        size > elf32::kUserAddressSpaceSize - offset) {
        return false;
    }
    copy_region(backing + offset, static_cast<const uint8_t*>(source), size);
    return true;
}

[[nodiscard]] bool write_initial_u32(uint8_t* backing,
                                     uint32_t user_address,
                                     uint32_t value) noexcept {
    return write_initial_bytes(backing, user_address, &value, sizeof(value));
}

template <uint32_t ArgCount, uint32_t EnvCount>
[[nodiscard]] bool build_initial_user_stack(uint8_t* backing,
                                            const ExecVector<ArgCount>& argv,
                                            const ExecVector<EnvCount>& envp,
                                            uint32_t* out_stack_pointer) noexcept {
    if (backing == nullptr || out_stack_pointer == nullptr || argv.count == 0U) {
        return false;
    }

    uint32_t stack_pointer = elf32::kUserVirtualBase + elf32::kUserAddressSpaceSize;
    uint32_t argv_addresses[ArgCount];
    uint32_t envp_addresses[EnvCount];
    initialize_pointer_vector(argv_addresses, 0U);
    initialize_pointer_vector(envp_addresses, 0U);

    for (uint32_t index = envp.count; index > 0U; --index) {
        const char* text = envp.values[index - 1U];
        const uint32_t length = string_length(text) + 1U;
        stack_pointer -= length;
        if (!write_initial_bytes(backing, stack_pointer, text, length)) {
            return false;
        }
        envp_addresses[index - 1U] = stack_pointer;
    }

    for (uint32_t index = argv.count; index > 0U; --index) {
        const char* text = argv.values[index - 1U];
        const uint32_t length = string_length(text) + 1U;
        stack_pointer -= length;
        if (!write_initial_bytes(backing, stack_pointer, text, length)) {
            return false;
        }
        argv_addresses[index - 1U] = stack_pointer;
    }

    stack_pointer = align_down(stack_pointer, sizeof(uint32_t));

    stack_pointer -= static_cast<uint32_t>(4U * sizeof(uint32_t));
    const uint32_t auxv[4] = {
        kAuxvTagPageSize,
        kPageSize,
        kAuxvTagNull,
        0U,
    };
    if (!write_initial_bytes(backing,
                          stack_pointer,
                          auxv,
                          static_cast<uint32_t>(sizeof(auxv)))) {
        return false;
    }

    stack_pointer -= static_cast<uint32_t>((envp.count + 1U) * sizeof(uint32_t));
    for (uint32_t index = 0U; index < envp.count; ++index) {
        if (!write_initial_u32(backing,
                            stack_pointer + (index * sizeof(uint32_t)),
                            envp_addresses[index])) {
            return false;
        }
    }
    if (!write_initial_u32(backing,
                        stack_pointer + (envp.count * sizeof(uint32_t)),
                        0U)) {
        return false;
    }

    stack_pointer -= static_cast<uint32_t>((argv.count + 1U) * sizeof(uint32_t));
    for (uint32_t index = 0U; index < argv.count; ++index) {
        if (!write_initial_u32(backing,
                            stack_pointer + (index * sizeof(uint32_t)),
                            argv_addresses[index])) {
            return false;
        }
    }
    if (!write_initial_u32(backing,
                        stack_pointer + (argv.count * sizeof(uint32_t)),
                        0U)) {
        return false;
    }

    stack_pointer -= sizeof(uint32_t);
    if (!write_initial_u32(backing, stack_pointer, argv.count)) {
        return false;
    }

    *out_stack_pointer = stack_pointer;
    return true;
}

enum class ImageLoadStatus : uint8_t {
    Okay,
    InvalidImage,
    NoMemory,
};

[[nodiscard]] ImageLoadStatus load_process_image(Process* process,
                                      const bootfs::FileRecord* file,
                                      const ExecVector<kMaxExecArgs>& argv,
                                      const ExecVector<kMaxExecEnvs>& envp) noexcept {
    if (process == nullptr || process->address_space == nullptr || file == nullptr) {
        return ImageLoadStatus::InvalidImage;
    }
    uint8_t* candidate = user_backing::acquire();
    if (candidate == nullptr) {
        return ImageLoadStatus::NoMemory;
    }
    elf32::UserImage image{};
    if (!elf32::load_static_image(file->data,
                                  file->size,
                                  candidate,
                                  elf32::kUserAddressSpaceSize,
                                  &image)) {
        static_cast<void>(user_backing::release(candidate));
        return ImageLoadStatus::InvalidImage;
    }
    if (!build_initial_user_stack(candidate, argv, envp, &image.stack_top)) {
        static_cast<void>(user_backing::release(candidate));
        return ImageLoadStatus::InvalidImage;
    }
    uint8_t* previous = process->address_space;
    process->address_space = candidate;
    process->segment_base = compute_segment_base(*process);
    zero_region(reinterpret_cast<uint8_t*>(process->mappings),
                static_cast<uint32_t>(sizeof(process->mappings)));
    initialize_context(process, image, 0U);
    process->minimum_break = image.brk_start;
    process->current_break = image.brk_start;
    static_cast<void>(user_backing::release(previous));
    return ImageLoadStatus::Okay;
}

void apply_service_profile_to_process(Process* process,
                                      const SupervisedService* service) noexcept {
    if (process == nullptr || service == nullptr) {
        return;
    }
    apply_scheduler_profile(process,
                            service->priority,
                            service->base_priority,
                            service->quantum_ticks,
                            service->scheduler_domain);
}

// -- getdents helper structs and functions ---------------------------------

struct LinuxDirent32 {
    uint32_t d_ino;
    uint32_t d_off;
    uint16_t d_reclen;
    char d_name[1]; // variable-length
};

struct GetdentsContext {
    Process* process;
    uint32_t user_buffer;
    uint32_t buffer_size;
    uint32_t offset;
    uint32_t entry_count;
    const char* dir_path;
};

bool starts_with_dir(const char* path, const char* dir) noexcept {
    if (path == nullptr || dir == nullptr) {
        return false;
    }
    while (*dir != '\0') {
        if (*path != *dir) {
            return false;
        }
        ++path;
        ++dir;
    }
    return true;
}

// Extract the immediate child name from a path relative to dir_path.
// E.g., dir_path="/bin", entry="/bin/sh" -> "sh"
// Returns false if not an immediate child.
bool extract_child_name(const char* entry_path,
                        const char* dir_path,
                        char* name_out,
                        uint32_t capacity) noexcept {
    if (entry_path == nullptr || dir_path == nullptr || name_out == nullptr || capacity == 0U) {
        return false;
    }
    const uint32_t dir_len = string_length(dir_path);
    if (!starts_with_dir(entry_path, dir_path)) {
        return false;
    }
    const char* rest = entry_path + dir_len;
    if (*rest == '/') {
        ++rest;
    }
    if (*rest == '\0') {
        return false; // This IS the directory, not a child
    }
    // Check it's an immediate child (no further '/' except trailing)
    uint32_t len = 0U;
    while (rest[len] != '\0' && rest[len] != '/') {
        if (len + 1U >= capacity) {
            return false;
        }
        name_out[len] = rest[len];
        ++len;
    }
    name_out[len] = '\0';
    // If there's more after the slash, it's a deeper entry
    if (rest[len] == '/' && rest[len + 1U] != '\0') {
        return false;
    }
    return len > 0U;
}

bool getdents_visitor(const bootfs::FileRecord& file, void* context) noexcept {
    auto* ctx = static_cast<GetdentsContext*>(context);
    if (ctx == nullptr || file.path == nullptr) {
        return true;
    }

    char name[64]{};
    if (!extract_child_name(file.path, ctx->dir_path, name, sizeof(name))) {
        return true; // Not a child of this directory, skip
    }

    const uint32_t name_len = string_length(name);
    const uint32_t reclen = align_up(
        static_cast<uint32_t>(sizeof(uint32_t) * 2U + sizeof(uint16_t) + name_len + 2U),
        4U);

    if (ctx->offset + reclen > ctx->buffer_size) {
        return false; // Buffer full, stop
    }

    uint8_t* dest = nullptr;
    if (!translate_user_region(ctx->process, ctx->user_buffer + ctx->offset, reclen, &dest)) {
        return false;
    }

    // Write d_ino (use entry_count+2 as fake inode)
    auto* ino = reinterpret_cast<uint32_t*>(dest);
    *ino = ctx->entry_count + 2U;
    // Write d_off (next offset)
    auto* off = reinterpret_cast<uint32_t*>(dest + 4U);
    *off = ctx->offset + reclen;
    // Write d_reclen
    auto* recl = reinterpret_cast<uint16_t*>(dest + 8U);
    *recl = static_cast<uint16_t>(reclen);
    // Write d_name
    char* name_dest = reinterpret_cast<char*>(dest + 10U);
    copy_region(reinterpret_cast<uint8_t*>(name_dest),
                reinterpret_cast<const uint8_t*>(name),
                name_len + 1U);
    // Write d_type byte after null terminator
    dest[10U + name_len + 1U] = file.is_directory ? 4U : 8U; // DT_DIR=4, DT_REG=8

    ctx->offset += reclen;
    ++ctx->entry_count;
    return true;
}

} // anonymous namespace

// -- Supervised service management -----------------------------------------

[[nodiscard]] bool is_compatible_user_image(const bootfs::FileRecord* file) noexcept {
    if (file == nullptr || !file->executable || file->data == nullptr) {
        return false;
    }
    elf32::UserImage image{};
    return elf32::inspect_static_image(file->data, file->size, &image);
}

[[nodiscard]] const bootfs::FileRecord* select_init_shell(const char** out_path,
                                                          const char** out_env) noexcept {
    struct Candidate {
        const char* path;
        const char* env;
    };

    static constexpr Candidate kCandidates[] = {
        {"/bin/mksh", "SHELL=/bin/mksh"},
        {"/bin/sh", "SHELL=/bin/sh"},
    };

    for (const Candidate& candidate : kCandidates) {
        const bootfs::FileRecord* file = bootfs::find(candidate.path);
        if (!is_compatible_user_image(file)) {
            continue;
        }
        *out_path = candidate.path;
        *out_env = candidate.env;
        return file;
    }
    return nullptr;
}

[[nodiscard]] SupervisedService* allocate_supervised_service() noexcept {
    for (auto& service : g_supervised_services) {
        if (!service.in_use) {
            zero_region(reinterpret_cast<uint8_t*>(&service),
                        static_cast<uint32_t>(sizeof(service)));
            service.in_use = true;
            service.service_id = g_next_service_id++;
            return &service;
        }
    }
    return nullptr;
}

[[nodiscard]] SupervisedService* find_supervised_service_by_process(
    const Process* process) noexcept {
    if (process == nullptr) {
        return nullptr;
    }
    for (auto& service : g_supervised_services) {
        if (service.in_use && service.pid == process->pid) {
            return &service;
        }
    }
    return nullptr;
}

[[nodiscard]] SupervisedService* find_supervised_service_by_dag_index(int dag_index) noexcept {
    if (dag_index < 0) {
        return nullptr;
    }
    for (auto& service : g_supervised_services) {
        if (service.in_use && service.dag_index == dag_index) {
            return &service;
        }
    }
    return nullptr;
}

[[nodiscard]] xinim::kernel::recovery::ServiceNode* dag_node_for_service(
    SupervisedService* service) noexcept {
    if (service == nullptr || service->dag_index < 0) {
        return nullptr;
    }
    return g_service_recovery_dag.get_service_mut(service->dag_index);
}

void sync_service_from_dag(SupervisedService* service) noexcept {
    xinim::kernel::recovery::ServiceNode* node = dag_node_for_service(service);
    if (service == nullptr || node == nullptr) {
        return;
    }
    service->restart_policy = node->policy;
    service->state = node->state;
    service->restart_count = node->restart_count;
    service->max_restarts = node->max_restarts;
    service->pid = node->pid >= 0 ? static_cast<uint32_t>(node->pid) : 0U;
}

void set_service_state(SupervisedService* service,
                       xinim::kernel::recovery::ServiceState state,
                       int32_t pid = -1) noexcept {
    xinim::kernel::recovery::ServiceNode* node = dag_node_for_service(service);
    if (node != nullptr) {
        g_service_recovery_dag.set_state(service->dag_index, state);
        if (pid >= 0) {
            g_service_recovery_dag.set_pid(service->dag_index, pid);
        } else {
            g_service_recovery_dag.set_pid(service->dag_index, -1);
        }
    }
    if (service != nullptr) {
        service->state = state;
        service->pid = pid >= 0 ? static_cast<uint32_t>(pid) : 0U;
        sync_service_from_dag(service);
    }
}

[[nodiscard]] SupervisedService* register_supervised_service(
    const char* name,
    const bootfs::FileRecord* file,
    const char* path,
    const char* env,
    xinim::kernel::recovery::RestartPolicy restart_policy,
    uint8_t max_restarts,
    bool respawn_on_clean_exit,
    Process* process,
    ServiceLaunchMode launch_mode = ServiceLaunchMode::BootOnly,
    uint32_t priority = kInitServicePriority,
    uint32_t quantum_ticks = 0U,
    uint16_t scheduler_domain = 1U) noexcept {
    SupervisedService* service = allocate_supervised_service();
    if (service == nullptr) {
        return nullptr;
    }
    const int dag_index = g_service_recovery_dag.add_service(
        name != nullptr ? name : "service",
        restart_policy,
        max_restarts);
    if (dag_index < 0) {
        service->in_use = false;
        return nullptr;
    }
    copy_c_string(service->name,
                  static_cast<uint32_t>(sizeof(service->name)),
                  name != nullptr ? name : "service");
    service->file = file;
    service->path = path;
    service->env = env;
    service->pid = process != nullptr ? process->pid : 0U;
    service->restart_policy = restart_policy;
    service->state = xinim::kernel::recovery::ServiceState::STARTING;
    service->restart_count = 0U;
    service->max_restarts = max_restarts;
    service->respawn_on_clean_exit = respawn_on_clean_exit;
    service->dag_index = dag_index;
    service->launch_mode = launch_mode;
    service->priority = xinim::kernel::sched_policy::clamp_priority(priority);
    service->base_priority = service->priority;
    service->quantum_ticks = quantum_ticks;
    service->scheduler_domain = scheduler_domain;
    set_service_state(service,
                      process != nullptr ? xinim::kernel::recovery::ServiceState::STARTING
                                         : xinim::kernel::recovery::ServiceState::STOPPED,
                      process != nullptr ? static_cast<int32_t>(process->pid) : -1);
    return service;
}

[[nodiscard]] bool build_service_vectors(const SupervisedService* service,
                                         ExecVector<kMaxExecArgs>* argv,
                                         ExecVector<kMaxExecEnvs>* envp) noexcept {
    if (service == nullptr || argv == nullptr || envp == nullptr || service->path == nullptr ||
        service->env == nullptr) {
        return false;
    }
    zero_region(reinterpret_cast<uint8_t*>(argv), static_cast<uint32_t>(sizeof(*argv)));
    zero_region(reinterpret_cast<uint8_t*>(envp), static_cast<uint32_t>(sizeof(*envp)));
    return append_exec_string(argv, service->path) &&
           append_exec_string(envp, "PATH=/bin:/usr/bin") &&
           append_exec_string(envp, "HOME=/") &&
           append_exec_string(envp, "TERM=vt100") &&
           append_exec_string(envp, "PS1=$ ") &&
           append_exec_string(envp, service->env);
}

[[nodiscard]] bool can_restart_service(const SupervisedService* service,
                                       bool crashed) noexcept {
    if (service == nullptr) {
        return false;
    }
    using xinim::kernel::recovery::RestartPolicy;
    switch (service->restart_policy) {
    case RestartPolicy::IGNORE:
        return false;
    case RestartPolicy::PANIC:
        return false;
    case RestartPolicy::RESTART:
    case RestartPolicy::KILL_DEPS:
        break;
    }
    if (!crashed && !service->respawn_on_clean_exit) {
        return false;
    }
    return service->max_restarts == 0U || service->restart_count <= service->max_restarts;
}

[[nodiscard]] int plan_service_restart(SupervisedService* service,
                                       bool crashed,
                                       int restart_order[],
                                       int max_order) noexcept {
    if (service == nullptr || restart_order == nullptr || max_order <= 0) {
        return 0;
    }
    if (crashed) {
        return g_service_recovery_dag.notify_crash(service->dag_index, restart_order, max_order);
    }

    xinim::kernel::recovery::ServiceNode* node = dag_node_for_service(service);
    if (node == nullptr) {
        return 0;
    }
    g_service_recovery_dag.set_state(service->dag_index, xinim::kernel::recovery::ServiceState::STOPPED);
    g_service_recovery_dag.set_pid(service->dag_index, -1);
    node->restart_count++;
    sync_service_from_dag(service);
    if (!can_restart_service(service, false)) {
        return 0;
    }
    restart_order[0] = service->dag_index;
    return 1;
}

[[nodiscard]] bool add_service_dependency(SupervisedService* dependent,
                                          const SupervisedService* dependency) noexcept {
    if (dependent == nullptr || dependency == nullptr) {
        return false;
    }
    return g_service_recovery_dag.add_dependency(dependent->dag_index, dependency->dag_index);
}

[[nodiscard]] bool prepare_supervised_service_process(SupervisedService* service,
                                                      Process* process) noexcept {
    if (service == nullptr || process == nullptr || service->file == nullptr) {
        return false;
    }

    ExecVector<kMaxExecArgs> argv{};
    ExecVector<kMaxExecEnvs> envp{};
    if (!build_service_vectors(service, &argv, &envp) ||
        load_process_image(process, service->file, argv, envp) != ImageLoadStatus::Okay) {
        return false;
    }

    reset_fd_map_to_console(process);
    initialize_supervised_session(*process, string_equals(service->name, "init-shell"));
    init_signal_state(process);
    process->state = ProcessState::Runnable;
    process->exit_status = 0U;
    process->saved_kernel_esp = 0U;
    process->wait_reason = WaitReason::None;
    process->wake_tick = 0U;
    apply_service_profile_to_process(process, service);
    set_service_state(service,
                      xinim::kernel::recovery::ServiceState::RUNNING,
                      static_cast<int32_t>(process->pid));
    return true;
}

[[nodiscard]] Process* process_for_service(SupervisedService* service,
                                           Process* preferred) noexcept {
    if (preferred != nullptr) {
        return preferred;
    }
    if (service != nullptr && service->pid != 0U) {
        Process* existing = find_process(service->pid);
        if (existing != nullptr) {
            return existing;
        }
    }
    return allocate_process(0U);
}

[[nodiscard]] bool restart_services_in_order(const int restart_order[],
                                             int restart_count,
                                             SupervisedService* crashed_service,
                                             Process* crashed_process) noexcept {
    if (restart_order == nullptr || restart_count <= 0) {
        return false;
    }

    for (int index = 0; index < restart_count; ++index) {
        SupervisedService* service = find_supervised_service_by_dag_index(restart_order[index]);
        if (service == nullptr) {
            return false;
        }

        Process* target_process =
            (service == crashed_service) ? crashed_process : process_for_service(service, nullptr);
        if (target_process == nullptr) {
            return false;
        }

        set_service_state(service,
                          xinim::kernel::recovery::ServiceState::RESTARTING,
                          static_cast<int32_t>(target_process->pid));
        if (!prepare_supervised_service_process(service, target_process)) {
            return false;
        }
    }

    return true;
}

[[noreturn]] void resume_supervised_service(SupervisedService* service,
                                            Process* process,
                                            const char* banner) noexcept {
    if (service == nullptr || process == nullptr) {
        resume_rescue_shell("Ring 3 supervised service reload failed");
    }
    if (!prepare_supervised_service_process(service, process)) {
        resume_rescue_shell("Ring 3 supervised service reload failed");
    }
    console::write_string(banner);
    console::newline();

    dispatch_process(process);
}

[[nodiscard]] SupervisedService* register_optional_support_services(
    SupervisedService* init_service,
    Process** out_process) noexcept {
    if (out_process != nullptr) {
        *out_process = nullptr;
    }
    const bootfs::FileRecord* hold_file = bootfs::find("/bin/holdsvc");
    if (!is_compatible_user_image(hold_file)) {
        return nullptr;
    }
    Process* hold_process = allocate_process(0U);
    if (hold_process == nullptr) {
        return nullptr;
    }
    SupervisedService* hold_service = register_supervised_service(
        "hold-service",
        hold_file,
        "/bin/holdsvc",
        "SERVICE=hold-service",
        xinim::kernel::recovery::RestartPolicy::RESTART,
        kHoldServiceMaxRestarts,
        false,
        hold_process,
        ServiceLaunchMode::BootOnly,
        kSupportServicePriority,
        0U,
        1U);
    if (hold_service == nullptr) {
        destroy_process(hold_process);
        return nullptr;
    }
    hold_service->owner_pid = init_service != nullptr ? init_service->pid : 0U;
    if (init_service != nullptr) {
        static_cast<void>(add_service_dependency(hold_service, init_service));
    }
    if (!prepare_supervised_service_process(hold_service, hold_process)) {
        destroy_process(hold_process);
        set_service_state(hold_service, xinim::kernel::recovery::ServiceState::CRASHED);
        return nullptr;
    }
    if (out_process != nullptr) {
        *out_process = hold_process;
    }
    console::write_string("Prepared supervised support service hold-service");
    console::newline();
    return hold_service;
}

// -- Process exit ----------------------------------------------------------

[[noreturn]] void exit_current_process(uint32_t exit_status,
                                       const char* fallback_reason,
                                       bool crashed = false) noexcept {
    Process* process = g_current_process;
    if (process == nullptr) {
        resume_rescue_shell(fallback_reason);
    }

#ifdef XINIM_X86_32_TTY_TRACE
    console::write_string("tty trace: exit pid=");
    console::write_dec32(process->pid);
    console::write_string(" status=");
    console::write_dec32(exit_status);
    console::write_string(" crashed=");
    console::write_dec32(crashed ? 1U : 0U);
    console::newline();
#endif

    process->exit_status = exit_status;
    process->state = ProcessState::Exited;

    // Release all fd_map entries, decrement refcounts on global slots
    for (int fdi = 0; fdi < kMaxFds; ++fdi) {
        const int slot = process->fd_map[fdi];
        if (slot >= 0) {
            process->fd_map[fdi] = -1;
            bootfs::decrement_slot_refcount(slot);
        }
    }

    release_controlling_terminal(*process);

    // Reparent orphaned children to PID 1 (init)
    for (auto& child : g_processes) {
        if (child.in_use && child.ppid == process->pid) {
            child.ppid = 1U;
        }
    }

    // Handle supervised service restart BEFORE notifying parent, since
    // resume_waiting_parent is [[noreturn]] and would skip restart logic.
    SupervisedService* service = find_supervised_service_by_process(process);
    if (service != nullptr) {
        int restart_order[kMaxSupervisedServices]{};
        set_service_state(service,
                          crashed ? xinim::kernel::recovery::ServiceState::CRASHED
                                  : xinim::kernel::recovery::ServiceState::STOPPED,
                          -1);
        const int restart_count = plan_service_restart(
            service,
            crashed,
            restart_order,
            static_cast<int>(kMaxSupervisedServices));
        sync_service_from_dag(service);
        if (restart_count > 1) {
            if (!restart_services_in_order(restart_order, restart_count, service, process)) {
                resume_rescue_shell("supervised multi-service restart failed");
            }
            console::write_string("Restarted supervised service set in DAG order");
            console::newline();
            dispatch_next_runnable("supervised service restart produced no runnable process");
        }
        if (restart_count == 1 && restart_order[0] == service->dag_index) {
            resume_supervised_service(service, process, "Respawning supervised service init-shell");
        }

        console::write_string("Supervised service restart limit reached: ");
        console::write_string(service->name);
        console::newline();
        resume_rescue_shell("supervised service restart limit reached");
    }

    Process* parent = find_process(process->ppid);
    if (parent != nullptr) {
        send_signal_to_process(parent, kSigChld);
        // SA_NOCLDWAIT: auto-reap child without zombie state
        const SignalHandler32& chld_handler = parent->signals.handlers[kSigChld];
        if ((chld_handler.flags & kSaNocldwait) != 0U) {
            destroy_process(process);
            dispatch_next_runnable("child auto-reaped via SA_NOCLDWAIT");
        }
        if (parent->state == ProcessState::Waiting) {
            resume_waiting_parent(parent);
        }
        dispatch_next_runnable("no runnable i486 process after child exit");
    }

    destroy_process(process);
    dispatch_next_runnable(fallback_reason);
}

[[nodiscard]] uint32_t wait_status_for_exit(uint32_t exit_status) noexcept {
    return (exit_status & 0xFFU) << 8U;
}

// -- Syscall implementations -----------------------------------------------

[[nodiscard]] uint32_t sys_read(Process* process, RegisterFrame* frame) noexcept {
    const int user_fd = static_cast<int>(frame->ebx);
    const int fd = resolve_fd(process, user_fd);
    const uint32_t count = frame->edx;
    if (count == 0U) {
        return 0U;
    }

    uint8_t* buffer = nullptr;
    if (!translate_user_region(process, frame->ecx, count, &buffer)) {
        return kErrnoFault;
    }

    // SIGTTIN: background process reading from console gets stopped
    const bool reading_console = (fd >= 0 && bootfs::is_console_fd(fd)) || (user_fd == 0 && fd < 0);
    if (reading_console && owns_controlling_terminal(*process)) {
        const int fg_pgrp = bootfs::foreground_pgrp();
        if (fg_pgrp > 0 && process->pgid != static_cast<uint32_t>(fg_pgrp)) {
            send_signal_to_process(process, kSigTtin);
            return kErrnoIntr;
        }
    }

    const bool is_open_fd = fd >= 0 && bootfs::is_open(fd);
    if (!is_open_fd) {
        if (user_fd != 0) {
            return kErrnoBadF;
        }
        uint32_t written = 0U;
        while (written < count) {
            char value = '\0';
            if (!console::tty_try_read_char(&value)) {
#ifdef XINIM_X86_32_TTY_TRACE
                console::write_string("tty trace: sys_read stdin block pid=");
                console::write_dec32(process->pid);
                console::newline();
#endif
                if (block_current_process_until_rescheduled(process, WaitReason::ConsoleInput, 0U)) {
                    return written > 0U ? written : kErrnoIntr;
                }
                continue;
            }
            buffer[written] = static_cast<uint8_t>(value);
            ++written;
            if (buffer[written - 1U] == '\r' || buffer[written - 1U] == '\n') {
                break;
            }
        }
        return written;
    }

    for (;;) {
        const int result = bootfs::read(fd, buffer, count);
        if (result == bootfs::kReadWouldBlock) {
#ifdef XINIM_X86_32_TTY_TRACE
            if (bootfs::is_console_fd(fd)) {
                console::write_string("tty trace: sys_read fd block pid=");
                console::write_dec32(process->pid);
                console::write_string(" fd=");
                console::write_dec32(static_cast<uint32_t>(fd));
                console::newline();
            }
#endif
            if (block_current_process_until_rescheduled(process, WaitReason::ConsoleInput, 0U)) {
                return kErrnoIntr; // Interrupted by signal
            }
            continue;
        }
#ifdef XINIM_X86_32_TTY_TRACE
        if (bootfs::is_console_fd(fd)) {
            console::write_string("tty trace: sys_read fd result=");
            console::write_dec32(result >= 0 ? static_cast<uint32_t>(result) : 0U);
            console::newline();
        }
#endif
        return result >= 0 ? static_cast<uint32_t>(result) : kErrnoBadF;
    }
}

[[nodiscard]] uint32_t sys_write(Process* process, RegisterFrame* frame) noexcept {
    const int user_fd = static_cast<int>(frame->ebx);
    const int fd = resolve_fd(process, user_fd);
    const uint32_t count = frame->edx;
    const bool is_open_fd = fd >= 0 && bootfs::is_open(fd);
    if (!is_open_fd) {
        if (user_fd != 1 && user_fd != 2) {
            return kErrnoBadF;
        }
        const uint8_t* buffer = nullptr;
        if (!translate_user_region(process,
                                   frame->ecx,
                                   count,
                                   const_cast<uint8_t**>(&buffer))) {
            return kErrnoFault;
        }
        for (uint32_t index = 0U; index < count; ++index) {
            console::tty_write_char(static_cast<char>(buffer[index]));
        }
        return count;
    }

    uint8_t* buffer = nullptr;
    if (!translate_user_region(process, frame->ecx, count, &buffer)) {
        return kErrnoFault;
    }

#ifdef XINIM_X86_32_TTY_TRACE
    static uint32_t g_write_trace_budget = 24U;
    if (bootfs::is_console_fd(fd) && g_write_trace_budget != 0U) {
        --g_write_trace_budget;
        console::write_string("tty trace: write pid=");
        console::write_dec32(process->pid);
        console::write_string(" userfd=");
        console::write_dec32(static_cast<uint32_t>(user_fd >= 0 ? user_fd : 0));
        console::write_string(" count=");
        console::write_dec32(count);
        console::write_string(" text=\"");
        const uint32_t preview_count = count < 96U ? count : 96U;
        for (uint32_t index = 0U; index < preview_count; ++index) {
            const char ch = static_cast<char>(buffer[index]);
            if (ch == '\n' || ch == '\r') {
                console::write_string("\\n");
            } else if (ch >= ' ' && ch <= '~') {
                console::write_char(ch);
            } else {
                console::write_char('.');
            }
        }
        console::write_string("\"");
        console::newline();
    }
#endif

    for (;;) {
        const int result = bootfs::write(fd, buffer, count);
        if (result == bootfs::kWriteWouldBlock) {
            if (block_current_process_until_rescheduled(process, WaitReason::PipeIO, 0U)) {
                return kErrnoIntr;
            }
            continue;
        }
        return result >= 0 ? static_cast<uint32_t>(result)
                           : static_cast<uint32_t>(result); // Preserve -EPIPE etc
    }
}

[[nodiscard]] uint32_t sys_dup(Process* process, RegisterFrame* frame) noexcept {
    const int user_fd = static_cast<int>(frame->ebx);
    const int global_slot = resolve_fd(process, user_fd);
    if (global_slot < 0 || !bootfs::is_open(global_slot)) {
        return kErrnoBadF;
    }
    const int new_local_fd = allocate_fd_map_entry(process, global_slot);
    if (new_local_fd < 0) {
        return kErrnoNoMem;
    }
    bootfs::increment_slot_refcount(global_slot);
    process->fd_flags[new_local_fd] = 0; // dup clears CLOEXEC
    return static_cast<uint32_t>(new_local_fd);
}

[[nodiscard]] uint32_t sys_dup2(Process* process, RegisterFrame* frame) noexcept {
    const int old_user_fd = static_cast<int>(frame->ebx);
    const int new_user_fd = static_cast<int>(frame->ecx);
    if (new_user_fd < 0 || new_user_fd >= kMaxFds) {
        return kErrnoInvalid;
    }
    const int global_slot = resolve_fd(process, old_user_fd);
    if (global_slot < 0 || !bootfs::is_open(global_slot)) {
        return kErrnoBadF;
    }
    if (old_user_fd == new_user_fd) {
        return static_cast<uint32_t>(new_user_fd);
    }
    // Close existing mapping at new_user_fd if any
    const int existing = process->fd_map[new_user_fd];
    if (existing >= 0) {
        bootfs::decrement_slot_refcount(existing);
    }
    process->fd_map[new_user_fd] = global_slot;
    bootfs::increment_slot_refcount(global_slot);
    process->fd_flags[new_user_fd] = 0; // dup2 clears CLOEXEC
    return static_cast<uint32_t>(new_user_fd);
}

[[nodiscard]] uint32_t sys_pipe(Process* process, RegisterFrame* frame) noexcept {
    int pipe_fds[2] = {-1, -1};
    const int result = bootfs::make_pipe(pipe_fds);
    if (result != 0) {
        return kErrnoNoMem;
    }
    // Map global pipe fds into process fd_map
    const int local_read = allocate_fd_map_entry(process, pipe_fds[0]);
    const int local_write = allocate_fd_map_entry(process, pipe_fds[1]);
    if (local_read < 0 || local_write < 0) {
        if (local_read >= 0) {
            process->fd_map[local_read] = -1;
        }
        bootfs::close(pipe_fds[0]);
        bootfs::close(pipe_fds[1]);
        return kErrnoNoMem;
    }
    if (!write_user_u32(process, frame->ebx, static_cast<uint32_t>(local_read)) ||
        !write_user_u32(process, frame->ebx + sizeof(uint32_t), static_cast<uint32_t>(local_write))) {
        process->fd_map[local_read] = -1;
        process->fd_map[local_write] = -1;
        bootfs::close(pipe_fds[0]);
        bootfs::close(pipe_fds[1]);
        return kErrnoFault;
    }
    return 0U;
}

[[nodiscard]] uint32_t sys_open(Process* process, RegisterFrame* frame) noexcept {
    char path[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
        return kErrnoFault;
    }
    if (string_equals(path, "/dev/tty") && !owns_controlling_terminal(*process)) {
        return static_cast<uint32_t>(-6); // ENXIO: caller has no controlling terminal.
    }
    const int global_slot = bootfs::open(path, frame->ecx, frame->edx);
#ifdef XINIM_X86_32_TTY_TRACE
    static uint32_t g_open_trace_budget = 48U;
    if (g_open_trace_budget != 0U) {
        --g_open_trace_budget;
        console::write_string("tty trace: open path=");
        console::write_string(path);
        console::write_string(" flags=");
        console::write_hex32(frame->ecx);
        console::write_string(" global=");
        console::write_dec32(global_slot >= 0 ? static_cast<uint32_t>(global_slot)
                                              : static_cast<uint32_t>(-global_slot));
        if (global_slot < 0) {
            console::write_string(" neg=yes");
        }
        console::newline();
    }
#endif
    if (global_slot < 0) {
        return static_cast<uint32_t>(global_slot);
    }
    const int local_fd = allocate_fd_map_entry(process, global_slot);
    if (local_fd < 0) {
        bootfs::close(global_slot);
        return kErrnoNoMem;
    }
    constexpr uint32_t open_no_controlling_terminal = 0x0100U;
    if (!owns_controlling_terminal(*process) && bootfs::is_console_fd(global_slot) &&
        (frame->ecx & open_no_controlling_terminal) == 0U) {
        static_cast<void>(acquire_controlling_terminal(*process));
    }
    return static_cast<uint32_t>(local_fd);
}

[[nodiscard]] uint32_t sys_lseek(Process* process, RegisterFrame* frame) noexcept {
    const int fd = resolve_fd(process, static_cast<int>(frame->ebx));
    if (fd < 0) {
        return kErrnoBadF;
    }
    const int64_t result = bootfs::seek(
        fd,
        static_cast<int32_t>(frame->ecx),
        static_cast<int>(frame->edx));
    return result >= 0 ? static_cast<uint32_t>(result) : kErrnoInvalid;
}

[[nodiscard]] uint32_t sys_stat(Process* process, RegisterFrame* frame) noexcept {
    char path[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
        return kErrnoFault;
    }

    bootfs::UserspaceStat host_stat{};
    if (bootfs::stat_path(path, &host_stat) != 0) {
        return kErrnoNoEnt;
    }

    uint8_t* destination = nullptr;
    if (!translate_user_region(process, frame->ecx, sizeof(host_stat), &destination)) {
        return kErrnoFault;
    }
    *reinterpret_cast<bootfs::UserspaceStat*>(destination) = host_stat;
    return 0U;
}

[[nodiscard]] uint32_t sys_fstat(Process* process, RegisterFrame* frame) noexcept {
    const int fd = resolve_fd(process, static_cast<int>(frame->ebx));
    if (fd < 0) {
        return kErrnoBadF;
    }
    bootfs::UserspaceStat host_stat{};
    if (bootfs::stat_fd(fd, &host_stat) != 0) {
        return kErrnoBadF;
    }

    uint8_t* destination = nullptr;
    if (!translate_user_region(process, frame->ecx, sizeof(host_stat), &destination)) {
        return kErrnoFault;
    }
    *reinterpret_cast<bootfs::UserspaceStat*>(destination) = host_stat;
    return 0U;
}

[[nodiscard]] uint32_t sys_fcntl(Process* process, RegisterFrame* frame) noexcept {
    const int user_fd = static_cast<int>(frame->ebx);
    const int fd = resolve_fd(process, user_fd);
    if (fd < 0) {
        return kErrnoBadF;
    }
    switch (static_cast<int>(frame->ecx)) {
    case kFcntlDupFd:
    case kFcntlDupFdCloexec: {
        const int minimum_fd = static_cast<int>(frame->edx);
        if (minimum_fd < 0 || minimum_fd >= kMaxFds) {
#ifdef XINIM_X86_32_TTY_TRACE
            console::write_string("tty trace: fcntl-dup-fail reason=min pid=");
            console::write_dec32(process->pid);
            console::write_string(" min=");
            console::write_dec32(static_cast<uint32_t>(minimum_fd));
            console::newline();
#endif
            return kErrnoInvalid;
        }
        if (!bootfs::is_open(fd)) {
#ifdef XINIM_X86_32_TTY_TRACE
            console::write_string("tty trace: fcntl-dup-fail reason=closed pid=");
            console::write_dec32(process->pid);
            console::write_string(" userfd=");
            console::write_dec32(static_cast<uint32_t>(user_fd));
            console::write_string(" fd=");
            console::write_dec32(fd >= 0 ? static_cast<uint32_t>(fd) : 0U);
            console::newline();
#endif
            return kErrnoBadF;
        }
        const int new_local = allocate_fd_map_entry_at_or_above(process, fd, minimum_fd);
        if (new_local < 0) {
#ifdef XINIM_X86_32_TTY_TRACE
            console::write_string("tty trace: fcntl-dup-fail reason=full pid=");
            console::write_dec32(process->pid);
            console::write_string(" min=");
            console::write_dec32(static_cast<uint32_t>(minimum_fd));
            console::write_string(" slots=");
            for (int slot = 8; slot < 16; ++slot) {
                console::write_dec32(static_cast<uint32_t>(slot));
                console::write_char(':');
                const int mapped = process->fd_map[slot];
                if (mapped < 0) {
                    console::write_string("-");
                } else {
                    console::write_dec32(static_cast<uint32_t>(mapped));
                }
                console::write_char(' ');
            }
            console::newline();
#endif
            return kErrnoNoMem;
        }
        bootfs::increment_slot_refcount(fd);
        process->fd_flags[new_local] =
            (static_cast<int>(frame->ecx) == kFcntlDupFdCloexec) ? kFdCloExec : 0;
#ifdef XINIM_X86_32_TTY_TRACE
        static uint32_t g_fcntl_dup_trace_budget = 32U;
        if (g_fcntl_dup_trace_budget != 0U) {
            --g_fcntl_dup_trace_budget;
            console::write_string("tty trace: fcntl-dup pid=");
            console::write_dec32(process->pid);
            console::write_string(" userfd=");
            console::write_dec32(static_cast<uint32_t>(user_fd));
            console::write_string(" min=");
            console::write_dec32(static_cast<uint32_t>(minimum_fd));
            console::write_string(" new=");
            console::write_dec32(static_cast<uint32_t>(new_local));
            console::write_string(" cloexec=");
            console::write_dec32(process->fd_flags[new_local] != 0 ? 1U : 0U);
            console::newline();
        }
#endif
        return static_cast<uint32_t>(new_local);
    }
    case kFcntlGetFd:
        return static_cast<uint32_t>(process->fd_flags[user_fd]);
    case kFcntlSetFd:
        process->fd_flags[user_fd] = static_cast<int>(frame->edx) & kFdCloExec;
        return 0U;
    case kFcntlGetFl: {
        const int result = bootfs::status_flags(fd);
        return result >= 0 ? static_cast<uint32_t>(result) : kErrnoBadF;
    }
    case kFcntlSetFl:
        return bootfs::set_status_flags(fd, static_cast<int>(frame->edx)) == 0
            ? 0U
            : kErrnoBadF;
    case kFcntlGetLk:
    case kFcntlSetLk:
    case kFcntlSetLkW: {
        uint8_t* fl_raw = nullptr;
        if (frame->edx == 0U || !translate_user_region(process, frame->edx,
                static_cast<uint32_t>(sizeof(lockf::Flock32)), &fl_raw)) {
            return kErrnoFault;
        }
        auto* fl = reinterpret_cast<lockf::Flock32*>(fl_raw);
        const int rc = lockf::do_fcntl_lock(fd, static_cast<int>(frame->ecx),
                                             fl, process->pid);
        return static_cast<uint32_t>(rc);
    }
    default:
        return kErrnoInvalid;
    }
}

[[nodiscard]] uint32_t sys_ioctl(Process* process, RegisterFrame* frame) noexcept {
    if (process == nullptr) {
        return kErrnoBadF;
    }
    const int fd = resolve_fd(process, static_cast<int>(frame->ebx));
    if (fd < 0) {
        return kErrnoBadF;
    }

    constexpr uint32_t tty_acquire = 0x540EU;
    constexpr uint32_t tty_get_group = 0x540FU;
    constexpr uint32_t tty_set_group = 0x5410U;
    constexpr uint32_t tty_detach = 0x5422U;
    constexpr uint32_t tty_get_session = 0x5429U;
    const uint32_t command = frame->ecx;
    if (command == tty_acquire || command == tty_detach || command == tty_get_group ||
        command == tty_set_group || command == tty_get_session) {
        if (!bootfs::is_console_fd(fd)) {
            return kErrnoNoTTY;
        }
        if (command == tty_acquire) {
            return acquire_controlling_terminal(*process);
        }
        if (!owns_controlling_terminal(*process)) {
            return kErrnoNoTTY;
        }
        if (command == tty_detach) {
            release_controlling_terminal(*process, true);
            return 0U;
        }
        uint8_t* translated = nullptr;
        if (!translate_user_region(process, frame->edx, sizeof(uint32_t), &translated)) {
            return kErrnoFault;
        }
        auto* value = reinterpret_cast<uint32_t*>(translated);
        if (command == tty_set_group) {
            return set_terminal_foreground(*process, static_cast<int32_t>(*value));
        }
        *value = command == tty_get_session ? process->session_id
                                            : static_cast<uint32_t>(bootfs::foreground_pgrp());
        return 0U;
    }

    const uintptr_t argument = static_cast<uintptr_t>(frame->edx);
    if (argument != 0U) {
        uint8_t* translated = nullptr;
        if (!translate_user_region(process, frame->edx, 128U, &translated)) {
            return kErrnoFault;
        }
        const int result = bootfs::control(
            fd,
            static_cast<int>(frame->ecx),
            reinterpret_cast<uintptr_t>(translated));
#ifdef XINIM_X86_32_TTY_TRACE
        static uint32_t g_ioctl_trace_budget = 32U;
        if (g_ioctl_trace_budget != 0U) {
            --g_ioctl_trace_budget;
            console::write_string("tty trace: ioctl pid=");
            console::write_dec32(process->pid);
            console::write_string(" userfd=");
            console::write_dec32(frame->ebx);
            console::write_string(" fd=");
            console::write_dec32(fd >= 0 ? static_cast<uint32_t>(fd) : 0U);
            console::write_string(" cmd=");
            console::write_hex32(frame->ecx);
            console::write_string(" result=");
            console::write_dec32(result == 0 ? 0U : static_cast<uint32_t>(-result));
            if (result != 0) {
                console::write_string(" neg=yes");
            }
            console::newline();
        }
#endif
        return result == 0 ? 0U : kErrnoNoTTY;
    }

    const int result = bootfs::control(
        fd,
        static_cast<int>(frame->ecx),
        0U);
#ifdef XINIM_X86_32_TTY_TRACE
    static uint32_t g_ioctl_null_trace_budget = 16U;
    if (g_ioctl_null_trace_budget != 0U) {
        --g_ioctl_null_trace_budget;
        console::write_string("tty trace: ioctl-null pid=");
        console::write_dec32(process->pid);
        console::write_string(" cmd=");
        console::write_hex32(frame->ecx);
        console::write_string(" result=");
        console::write_dec32(result == 0 ? 0U : static_cast<uint32_t>(-result));
        if (result != 0) {
            console::write_string(" neg=yes");
        }
        console::newline();
    }
#endif
    return result == 0 ? 0U : kErrnoNoTTY;
}

[[nodiscard]] uint32_t sys_access(Process* process, RegisterFrame* frame) noexcept {
    char path[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
        return kErrnoFault;
    }
    return static_cast<uint32_t>(bootfs::access(path));
}

[[nodiscard]] uint32_t sys_mkdir(Process* process, RegisterFrame* frame) noexcept {
    char path[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
        return kErrnoFault;
    }
    return bootfs::mkdir(path, frame->ecx) == 0 ? 0U : kErrnoNoEnt;
}

[[nodiscard]] uint32_t sys_rmdir(Process* process, RegisterFrame* frame) noexcept {
    char path[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
        return kErrnoFault;
    }
    return bootfs::rmdir(path) == 0 ? 0U : kErrnoNoEnt;
}

[[nodiscard]] uint32_t sys_rename(Process* process, RegisterFrame* frame) noexcept {
    char old_path[256]{};
    char new_path[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, old_path, sizeof(old_path)) ||
        !copy_and_resolve_user_path(process, frame->ecx, new_path, sizeof(new_path))) {
        return kErrnoFault;
    }
    return bootfs::rename(old_path, new_path) == 0 ? 0U : kErrnoNoEnt;
}

[[nodiscard]] uint32_t sys_unlink(Process* process, RegisterFrame* frame) noexcept {
    char path[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
        return kErrnoFault;
    }
    return bootfs::unlink(path) == 0 ? 0U : kErrnoNoEnt;
}

[[nodiscard]] uint32_t sys_chdir(Process* process, RegisterFrame* frame) noexcept {
    char path[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
        return kErrnoFault;
    }
    // Validate directory exists in bootfs or ext2
    if (string_equals(path, "/") || string_equals(path, ".")) {
        // Stay at current directory
        if (string_equals(path, "/")) {
            process->cwd[0] = '/';
            process->cwd[1] = '\0';
        }
        return 0U;
    }
    if (bootfs::is_directory(path)) {
        copy_c_string(process->cwd, static_cast<uint32_t>(sizeof(process->cwd)), path);
        return 0U;
    }
    return kErrnoNoEnt;
}

[[nodiscard]] uint32_t sys_getcwd(Process* process, RegisterFrame* frame) noexcept {
    const uint32_t cwd_len = string_length(process->cwd);
    uint8_t* buffer = nullptr;
    if (!translate_user_region(process, frame->ebx, frame->ecx, &buffer) ||
        frame->ecx < cwd_len + 1U) {
        return kErrnoFault;
    }
    copy_region(buffer, reinterpret_cast<const uint8_t*>(process->cwd), cwd_len + 1U);
    return cwd_len;
}

[[nodiscard]] uint32_t sys_fork(Process* process, RegisterFrame* frame) noexcept {
    Process* child = allocate_process(process->pid);
    if (child == nullptr) {
        return kErrnoNoMem;
    }
    copy_region(child->address_space, process->address_space, elf32::kUserAddressSpaceSize);
    child->minimum_break = process->minimum_break;
    child->current_break = process->current_break;
    copy_region(reinterpret_cast<uint8_t*>(child->mappings),
                reinterpret_cast<const uint8_t*>(process->mappings),
                static_cast<uint32_t>(sizeof(process->mappings)));
    apply_scheduler_profile(child,
                            process->priority,
                            process->base_priority,
                            process->quantum_ticks,
                            process->scheduler_domain);
    // Copy signal handlers from parent (pending/blocked are NOT inherited)
    copy_region(reinterpret_cast<uint8_t*>(child->signals.handlers),
                reinterpret_cast<const uint8_t*>(process->signals.handlers),
                static_cast<uint32_t>(sizeof(process->signals.handlers)));
    inherit_process_session(*child, *process);
    copy_c_string(child->cwd, static_cast<uint32_t>(sizeof(child->cwd)), process->cwd);
    // Copy per-process fd table and per-fd flags; increment refcounts on shared global slots
    for (int fdi = 0; fdi < kMaxFds; ++fdi) {
        if (child->fd_map[fdi] >= 0) {
            bootfs::decrement_slot_refcount(child->fd_map[fdi]);
        }
        child->fd_map[fdi] = process->fd_map[fdi];
        child->fd_flags[fdi] = process->fd_flags[fdi];
        if (process->fd_map[fdi] >= 0) {
            bootfs::increment_slot_refcount(process->fd_map[fdi]);
        }
    }
    child->context = capture_user_context(frame);
    child->context.eax = 0U;
    return child->pid;
}

[[noreturn]] void sys_execve(Process* process, RegisterFrame* frame) noexcept {
    char path[128]{};
    ExecVector<kMaxExecArgs> argv{};
    ExecVector<kMaxExecEnvs> envp{};
    if (!copy_user_string(process, frame->ebx, path, sizeof(path))) {
        process->context = capture_user_context(frame);
        process->context.eax = kErrnoFault;
        activate_process(process);
        i486_resume_user_context(&process->context);
        __builtin_unreachable();
    }
    if (!copy_exec_vector(process, frame->ecx, &argv, path) ||
        !copy_exec_vector(process, frame->edx, &envp)) {
        process->context = capture_user_context(frame);
        process->context.eax = kErrnoFault;
        activate_process(process);
        i486_resume_user_context(&process->context);
        __builtin_unreachable();
    }

    bootfs::FileRecord ext2_file{};
    const uint8_t* image = nullptr;
    uint32_t image_size = 0U;
    const bootfs::FileRecord* file = nullptr;
    if (ext2_reader::load_runtime_executable(path, &image, &image_size) && image != nullptr) {
        ext2_file = {
            .path = path,
            .data = const_cast<uint8_t*>(image),
            .size = image_size,
            .capacity = image_size,
            .read_only = true,
            .executable = true,
            .is_directory = false,
            .is_symlink = false,
            .user_id = 0U,
            .group_id = 0U,
            .exclusive_lock_owner = 0U,
            .shared_lock_count = 0U,
        };
        file = &ext2_file;
    } else {
        file = bootfs::find(path);
    }

    const ImageLoadStatus image_status =
        (file != nullptr && file->executable)
            ? load_process_image(process, file, argv, envp)
            : ImageLoadStatus::InvalidImage;
    if (image_status != ImageLoadStatus::Okay) {
#ifdef XINIM_X86_32_EXEC_IO_TRACE
        console::write_string("exec failure stage=");
        console::write_string(file == nullptr ? "lookup" :
                              (!file->executable ? "permission" : "image-load"));
        console::write_string(" path=");
        console::write_string(path);
        console::write_string(" pid=");
        console::write_dec32(process->pid);
        console::newline();
#endif
        process->context = capture_user_context(frame);
        process->context.eax = image_status == ImageLoadStatus::NoMemory
                                   ? kErrnoNoMem : kErrnoNoEnt;
        activate_process(process);
        i486_resume_user_context(&process->context);
        __builtin_unreachable();
    }

    process->executed_since_fork = true;
    // Record the executable path for /proc/PID/exe
    {
        uint32_t pi = 0U;
        while (path[pi] != '\0' && pi + 1U < sizeof(process->exe_path)) {
            process->exe_path[pi] = path[pi]; ++pi;
        }
        process->exe_path[pi] = '\0';
    }

    // POSIX: execve resets caught signal handlers to SIG_DFL, preserves SIG_IGN
    for (uint32_t sig = 1U; sig < kMaxSignals; ++sig) {
        if (process->signals.handlers[sig].handler != kSigIgn) {
            process->signals.handlers[sig].handler = kSigDfl;
            process->signals.handlers[sig].flags = 0U;
            process->signals.handlers[sig].mask = 0U;
        }
    }
    process->signals.pending = 0U;
    process->signals.in_handler = false;

    // POSIX: execve closes FD_CLOEXEC file descriptors via per-process fd_flags
    for (int fdi = 0; fdi < kMaxFds; ++fdi) {
        const int slot = process->fd_map[fdi];
        if (slot < 0) {
            continue;
        }
        if ((process->fd_flags[fdi] & 1) != 0) { // FD_CLOEXEC
            process->fd_map[fdi] = -1;
            process->fd_flags[fdi] = 0;
            bootfs::decrement_slot_refcount(slot);
        }
    }

    activate_process(process);
    i486_resume_user_context(&process->context);
    __builtin_unreachable();
}

[[nodiscard]] uint32_t sys_wait4(Process* process, RegisterFrame* frame) noexcept {
    const int32_t requested_pid = static_cast<int32_t>(frame->ebx);
    const uint32_t options = frame->edx;

    if (requested_pid == 0 || requested_pid < kWaitPidAny) {
        return kErrnoInvalid;
    }

    const bool no_hang = (options & kWaitNoHang) != 0U;
    const bool wuntraced = (options & kWaitUntraced) != 0U;

    if (!has_child(process)) {
        return kErrnoChild;
    }

    if (requested_pid > 0 && find_child(process, requested_pid) == nullptr) {
        return kErrnoChild;
    }

    Process* child = find_waiting_child(process, requested_pid, wuntraced);
    while (child == nullptr) {
        if (no_hang) {
            return 0U;
        }
        if (has_interrupting_signal(*process)) {
            return kErrnoIntr;
        }
        static_cast<void>(block_current_process_until_rescheduled(
            process, WaitReason::ChildState, 0U));
        child = find_waiting_child(process, requested_pid, wuntraced);
        if (child == nullptr && has_interrupting_signal(*process)) {
            return kErrnoIntr;
        }
        if (child == nullptr &&
            (!has_child(process) ||
             (requested_pid > 0 && find_child(process, requested_pid) == nullptr))) {
            return kErrnoChild;
        }
    }

    if (frame->ecx != 0U) {
        uint32_t status = 0U;
        if (child->state == ProcessState::Stopped) {
            status = child->exit_status; // Already encoded as (sig << 8) | 0x7F
        } else {
            status = wait_status_for_exit(child->exit_status);
        }
        if (!write_user_u32(process, frame->ecx, status)) {
            return kErrnoFault;
        }
    }
    const uint32_t pid = child->pid;
    if (child->state == ProcessState::Exited) {
        destroy_process(child);
    }
    // Stopped children are NOT destroyed -- they can be continued with SIGCONT
    return pid;
}

[[nodiscard]] uint32_t sys_brk(Process* process, RegisterFrame* frame) noexcept {
    if (process == nullptr || frame == nullptr) {
        return 0U;
    }

    const uint32_t requested_break = frame->ebx;
    if (requested_break == 0U) {
        return process->current_break;
    }
    if (requested_break < process->minimum_break ||
        requested_break > (elf32::kUserVirtualBase + elf32::kUserAddressSpaceSize)) {
        return process->current_break;
    }

    uint32_t stack_limit = compute_user_stack_limit(process);
    const uint32_t mapping_limit = lowest_mapping_base(process);
    if (mapping_limit < stack_limit) {
        stack_limit = mapping_limit;
    }
    if (stack_limit < process->minimum_break || requested_break > stack_limit) {
        return process->current_break;
    }

    if (requested_break > process->current_break) {
        uint8_t* region = nullptr;
        if (!translate_user_region(process,
                                   process->current_break,
                                   requested_break - process->current_break,
                                   &region)) {
            return process->current_break;
        }
        zero_region(region, requested_break - process->current_break);
    } else if (requested_break < process->current_break) {
        uint8_t* region = nullptr;
        if (!translate_user_region(process,
                                   requested_break,
                                   process->current_break - requested_break,
                                   &region)) {
            return process->current_break;
        }
        zero_region(region, process->current_break - requested_break);
    }

    process->current_break = requested_break;
    return process->current_break;
}

[[nodiscard]] uint32_t sys_mmap(Process* process, RegisterFrame* frame) noexcept {
    if (process == nullptr || frame == nullptr) {
        return kErrnoInvalid;
    }

    const uint32_t address = frame->ebx;
    const uint32_t length = align_up(frame->ecx, kPageSize);
    const uint32_t flags = frame->esi;
    const int32_t user_fd = static_cast<int32_t>(frame->edi);
    const uint32_t page_offset = frame->ebp;

    if (length == 0U) {
        return kErrnoInvalid;
    }
    if ((flags & kMapFixed) != 0U || address != 0U) {
        return kErrnoInvalid;
    }
    if ((flags & kMapPrivate) == 0U) {
        return kErrnoInvalid;
    }

    const bool anonymous = (flags & kMapAnonymous) != 0U;
    const bool file_backed = !anonymous && user_fd >= 0;

    // Resolve file descriptor for file-backed mapping
    int global_fd = -1;
    if (file_backed) {
        global_fd = resolve_fd(process, user_fd);
        if (global_fd < 0 || !bootfs::is_open(global_fd)) {
            return kErrnoBadF;
        }
    } else if (!anonymous) {
        return kErrnoInvalid;
    }

    UserMapping* slot = find_free_mapping_slot(process);
    if (slot == nullptr) {
        return kErrnoNoMem;
    }

    const uint32_t mapping_base = allocate_mapping_base(process, length);
    if (mapping_base == 0U) {
        return kErrnoNoMem;
    }

    uint8_t* region = nullptr;
    if (!translate_user_region(process, mapping_base, length, &region)) {
        return kErrnoNoMem;
    }
    zero_region(region, length);

    // For file-backed MAP_PRIVATE: read file contents into the mapping
    if (file_backed) {
        // Save and restore file position (pread semantics)
        const uint32_t offset = page_offset;
        const uint32_t to_read = (length < 65536U) ? length : 65536U;
        // Read via bootfs pread-style interface
        const int32_t old_pos = static_cast<int32_t>(bootfs::seek(global_fd, 0, 1)); // SEEK_CUR
        if (old_pos >= 0) {
            bootfs::seek(global_fd, static_cast<int32_t>(offset), 0); // SEEK_SET
            bootfs::read(global_fd, region, to_read);
            bootfs::seek(global_fd, old_pos, 0); // restore
        }
    }

    slot->in_use = true;
    slot->address = mapping_base;
    slot->size = length;
#ifdef XINIM_X86_32_TTY_TRACE
    static uint32_t g_mmap_trace_budget = 48U;
    if (g_mmap_trace_budget != 0U) {
        --g_mmap_trace_budget;
        console::write_string("tty trace: mmap pid=");
        console::write_dec32(process->pid);
        console::write_string(" len=");
        console::write_hex32(length);
        console::write_string(" prot=");
        console::write_hex32(frame->edx);
        console::write_string(" flags=");
        console::write_hex32(flags);
        console::write_string(" userfd=");
        console::write_dec32(user_fd >= 0 ? static_cast<uint32_t>(user_fd) : 0U);
        if (user_fd < 0) {
            console::write_string(" negfd=yes");
        }
        console::write_string(" result=");
        console::write_hex32(mapping_base);
        console::newline();
    }
#endif
    return mapping_base;
}

[[nodiscard]] uint32_t sys_munmap(Process* process, RegisterFrame* frame) noexcept {
    if (process == nullptr || frame == nullptr) {
        return kErrnoInvalid;
    }

    const uint32_t address = align_down(frame->ebx, kPageSize);
    const uint32_t length = align_up(frame->ecx, kPageSize);
    if (length == 0U) {
        return kErrnoInvalid;
    }

    UserMapping* mapping = find_mapping(process, address, length);
    if (mapping == nullptr) {
        return kErrnoInvalid;
    }

    uint8_t* region = nullptr;
    if (!translate_user_region(process, mapping->address, mapping->size, &region)) {
        return kErrnoInvalid;
    }
    zero_region(region, mapping->size);
    mapping->in_use = false;
    mapping->address = 0U;
    mapping->size = 0U;
    return 0U;
}

[[nodiscard]] uint32_t sys_mremap(Process* process, RegisterFrame* frame) noexcept {
    if (process == nullptr || frame == nullptr) {
        return kErrnoInvalid;
    }

    const uint32_t old_address = align_down(frame->ebx, kPageSize);
    const uint32_t old_size = align_up(frame->ecx, kPageSize);
    const uint32_t new_size = align_up(frame->edx, kPageSize);
    const uint32_t flags = frame->esi;
    if (old_size == 0U || new_size == 0U || (flags & ~kMremapMayMove) != 0U) {
        return kErrnoInvalid;
    }

    UserMapping* mapping = find_mapping(process, old_address, old_size);
    if (mapping == nullptr) {
        return kErrnoInvalid;
    }

    uint8_t* old_region = nullptr;
    if (!translate_user_region(process, mapping->address, mapping->size, &old_region)) {
        return kErrnoInvalid;
    }

    if (new_size <= old_size) {
        if (new_size < old_size) {
            zero_region(old_region + new_size, old_size - new_size);
        }
        mapping->size = new_size;
        return old_address;
    }

    if ((flags & kMremapMayMove) == 0U) {
        return kErrnoNoMem;
    }

    mapping->in_use = false;
    const uint32_t new_address = allocate_mapping_base(process, new_size);
    mapping->in_use = true;
    if (new_address == 0U) {
        return kErrnoNoMem;
    }

    uint8_t* new_region = nullptr;
    if (!translate_user_region(process, new_address, new_size, &new_region)) {
        return kErrnoNoMem;
    }
    copy_region(new_region, old_region, old_size);
    zero_region(new_region + old_size, new_size - old_size);
    const uint32_t old_end = old_address + old_size;
    const uint32_t new_end = new_address + new_size;
    const bool overlaps = new_address < old_end && old_address < new_end;
    if (!overlaps) {
        zero_region(old_region, old_size);
    }
    mapping->address = new_address;
    mapping->size = new_size;
    return new_address;
}

[[nodiscard]] uint32_t sys_mprotect_compat(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    (void)frame;
    return 0U;
}

// -- Time syscalls ---------------------------------------------------------

[[nodiscard]] uint32_t sys_time_compat(Process* process, RegisterFrame* frame) noexcept {
    const uint32_t now = current_epoch_seconds();
    if (frame->ebx != 0U && !write_user_u32(process, frame->ebx, now)) {
        return kErrnoFault;
    }
    return now;
}

[[nodiscard]] uint32_t sys_gettimeofday_compat(Process* process, RegisterFrame* frame) noexcept {
    if (frame->ebx != 0U) {
        const TimeVal32 value{current_epoch_seconds(), current_epoch_microseconds()};
        if (!write_user_bytes(process, frame->ebx, &value, static_cast<uint32_t>(sizeof(value)))) {
            return kErrnoFault;
        }
    }
    if (frame->ecx != 0U) {
        const uint32_t timezone[2] = {0U, 0U};
        if (!write_user_bytes(process, frame->ecx, timezone, static_cast<uint32_t>(sizeof(timezone)))) {
            return kErrnoFault;
        }
    }
    return 0U;
}

[[nodiscard]] uint32_t sys_clock_gettime_compat(Process* process, RegisterFrame* frame) noexcept {
    if (frame->ecx != 0U) {
        const uint64_t elapsed_ticks = g_scheduler_ticks - g_boot_epoch_ticks;
        const uint32_t elapsed_sec = static_cast<uint32_t>(elapsed_ticks / kTimerHz);
        const uint32_t sub_ticks = static_cast<uint32_t>(elapsed_ticks % kTimerHz);
        const uint32_t nsec = sub_ticks * (1000000000U / kTimerHz);
        // CLOCK_MONOTONIC (1) returns uptime; CLOCK_REALTIME (0) returns wall clock
        const uint32_t clock_id = frame->ebx;
        TimeSpec32 value{};
        if (clock_id == 0U) {
            value.seconds = g_boot_epoch_seconds + elapsed_sec;
            value.nanoseconds = nsec;
        } else {
            value.seconds = elapsed_sec;
            value.nanoseconds = nsec;
        }
        if (!write_user_bytes(process, frame->ecx, &value, static_cast<uint32_t>(sizeof(value)))) {
            return kErrnoFault;
        }
    }
    return 0U;
}

[[nodiscard]] uint32_t sys_alarm_compat(Process* process, RegisterFrame* frame) noexcept {
    const uint32_t seconds = frame->ebx;
    // Compute remaining time from previous alarm
    uint32_t remaining = 0U;
    if (process->alarm_tick > g_scheduler_ticks) {
        remaining = static_cast<uint32_t>(
            (process->alarm_tick - g_scheduler_ticks + kTimerHz - 1U) / kTimerHz);
    }
    // Set new alarm (0 = cancel)
    if (seconds == 0U) {
        process->alarm_tick = 0U;
    } else {
        process->alarm_tick = g_scheduler_ticks + static_cast<uint64_t>(seconds) * kTimerHz;
    }
    return remaining;
}

// -- setitimer / getitimer (POSIX interval timers) -----------------------

struct ITimerVal32 {
    uint32_t it_interval_sec;
    uint32_t it_interval_usec;
    uint32_t it_value_sec;
    uint32_t it_value_usec;
};

constexpr int kItimerReal = 0;
constexpr int kItimerVirtual = 1;
constexpr int kItimerProf = 2;

Process::IntervalTimer* itimer_for_which(Process* p, int which) noexcept {
    switch (which) {
    case kItimerReal: return &p->itimer_real;
    case kItimerVirtual: return &p->itimer_virtual;
    case kItimerProf: return &p->itimer_prof;
    default: return nullptr;
    }
}

uint64_t timeval_to_ticks(uint32_t sec, uint32_t usec) noexcept {
    return static_cast<uint64_t>(sec) * kTimerHz +
           static_cast<uint64_t>(usec) / (1000000U / kTimerHz);
}

void ticks_to_timeval(uint64_t ticks, uint32_t& sec, uint32_t& usec) noexcept {
    sec = static_cast<uint32_t>(ticks / kTimerHz);
    usec = static_cast<uint32_t>((ticks % kTimerHz) * (1000000U / kTimerHz));
}

[[nodiscard]] uint32_t sys_setitimer_impl(Process* process, RegisterFrame* frame) noexcept {
    const int which = static_cast<int>(frame->ebx);
    Process::IntervalTimer* timer = itimer_for_which(process, which);
    if (timer == nullptr) {
        return kErrnoInvalid;
    }

    // Old value output (optional, ecx)
    if (frame->edx != 0U) {
        ITimerVal32 old_val{};
        ticks_to_timeval(timer->interval, old_val.it_interval_sec, old_val.it_interval_usec);
        if (timer->deadline > g_scheduler_ticks) {
            ticks_to_timeval(timer->deadline - g_scheduler_ticks,
                             old_val.it_value_sec, old_val.it_value_usec);
        }
        if (!write_user_bytes(process, frame->edx, &old_val,
                              static_cast<uint32_t>(sizeof(old_val)))) {
            return kErrnoFault;
        }
    }

    // New value input (ecx)
    if (frame->ecx != 0U) {
        ITimerVal32 new_val{};
        uint8_t* src = nullptr;
        if (!translate_user_region(process, frame->ecx,
                                   static_cast<uint32_t>(sizeof(new_val)), &src)) {
            return kErrnoFault;
        }
        for (uint32_t i = 0U; i < sizeof(new_val); ++i) {
            reinterpret_cast<uint8_t*>(&new_val)[i] = src[i];
        }

        timer->interval = timeval_to_ticks(new_val.it_interval_sec, new_val.it_interval_usec);
        const uint64_t value_ticks = timeval_to_ticks(new_val.it_value_sec, new_val.it_value_usec);
        timer->deadline = (value_ticks > 0U) ? (g_scheduler_ticks + value_ticks) : 0U;

        // For ITIMER_REAL, also update legacy alarm_tick for backward compat
        if (which == kItimerReal) {
            process->alarm_tick = timer->deadline;
        }
    }
    return 0U;
}

[[nodiscard]] uint32_t sys_getitimer_impl(Process* process, RegisterFrame* frame) noexcept {
    const int which = static_cast<int>(frame->ebx);
    const Process::IntervalTimer* timer = itimer_for_which(process, which);
    if (timer == nullptr) {
        return kErrnoInvalid;
    }
    if (frame->ecx == 0U) {
        return kErrnoFault;
    }

    ITimerVal32 val{};
    ticks_to_timeval(timer->interval, val.it_interval_sec, val.it_interval_usec);
    if (timer->deadline > g_scheduler_ticks) {
        ticks_to_timeval(timer->deadline - g_scheduler_ticks,
                         val.it_value_sec, val.it_value_usec);
    }
    if (!write_user_bytes(process, frame->ecx, &val,
                          static_cast<uint32_t>(sizeof(val)))) {
        return kErrnoFault;
    }
    return 0U;
}

[[nodiscard]] uint32_t sys_getrlimit_compat(Process* process, RegisterFrame* frame) noexcept {
    if (frame->ecx == 0U) {
        return kErrnoFault;
    }
    const RLimit32 value{0xFFFFFFFFU, 0xFFFFFFFFU};
    if (!write_user_bytes(process, frame->ecx, &value, static_cast<uint32_t>(sizeof(value)))) {
        return kErrnoFault;
    }
    return 0U;
}

[[nodiscard]] uint32_t sys_setrlimit_compat(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    (void)frame;
    return 0U;
}

[[nodiscard]] uint32_t sys_getrusage_compat(Process* process, RegisterFrame* frame) noexcept {
    if (frame->ecx == 0U) {
        return kErrnoFault;
    }
    const RUsage32 value{};
    if (!write_user_bytes(process, frame->ecx, &value, static_cast<uint32_t>(sizeof(value)))) {
        return kErrnoFault;
    }
    return 0U;
}

[[nodiscard]] uint32_t sys_setsid_compat(Process* process, RegisterFrame* frame) noexcept {
    (void)frame;
    return create_process_session(*process);
}

[[nodiscard]] uint32_t sys_umask_compat(Process* process, RegisterFrame* frame) noexcept {
    const uint32_t previous = process->file_creation_mask;
    process->file_creation_mask = frame->ebx & 0777U;
    return previous;
}

[[nodiscard]] uint32_t sys_nice_compat(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    (void)frame;
    return 0U;
}

// -- getdents syscall ------------------------------------------------------

[[nodiscard]] uint32_t sys_getdents(Process* process, RegisterFrame* frame) noexcept {
    const int fd = resolve_fd(process, static_cast<int>(frame->ebx));
    const uint32_t user_buffer = frame->ecx;
    const uint32_t buffer_size = frame->edx;

    if (buffer_size < 32U) {
        return kErrnoInvalid;
    }

    // Get directory path from open fd
    const char* dir_path = bootfs::directory_path_for_fd(fd);
    if (dir_path == nullptr) {
        if (!bootfs::is_open(fd)) {
            return kErrnoBadF;
        }
        return kErrnoInvalid; // fd is not a directory
    }

    GetdentsContext ctx{};
    ctx.process = process;
    ctx.user_buffer = user_buffer;
    ctx.buffer_size = buffer_size;
    ctx.offset = 0U;
    ctx.entry_count = 0U;
    ctx.dir_path = dir_path;

    bootfs::for_each_entry(getdents_visitor, &ctx);

    // Also add entries from ext2 runtime if available
    char ext2_listing[512]{};
    const uint32_t ext2_len = ext2_reader::build_runtime_directory_listing(
        dir_path, ext2_listing, sizeof(ext2_listing));

    if (ext2_len > 0U) {
        // Parse newline-separated names from ext2 listing
        uint32_t pos = 0U;
        while (pos < ext2_len && ctx.offset < buffer_size) {
            char name[64]{};
            uint32_t name_len = 0U;
            bool is_dir = false;

            while (pos < ext2_len && ext2_listing[pos] != '\n' && ext2_listing[pos] != '\0') {
                if (name_len + 1U < sizeof(name)) {
                    name[name_len] = ext2_listing[pos];
                    ++name_len;
                }
                ++pos;
            }
            if (pos < ext2_len && ext2_listing[pos] == '\n') {
                ++pos;
            }
            if (name_len == 0U) {
                continue;
            }
            // Check for trailing '/' indicating directory
            if (name_len > 0U && name[name_len - 1U] == '/') {
                is_dir = true;
                --name_len;
            }
            name[name_len] = '\0';

            const uint32_t reclen = align_up(
                static_cast<uint32_t>(sizeof(uint32_t) * 2U + sizeof(uint16_t) + name_len + 2U),
                4U);
            if (ctx.offset + reclen > buffer_size) {
                break;
            }

            uint8_t* dest = nullptr;
            if (!translate_user_region(process, user_buffer + ctx.offset, reclen, &dest)) {
                break;
            }

            auto* ino = reinterpret_cast<uint32_t*>(dest);
            *ino = ctx.entry_count + 2U;
            auto* off = reinterpret_cast<uint32_t*>(dest + 4U);
            *off = ctx.offset + reclen;
            auto* recl = reinterpret_cast<uint16_t*>(dest + 8U);
            *recl = static_cast<uint16_t>(reclen);
            char* name_dest = reinterpret_cast<char*>(dest + 10U);
            copy_region(reinterpret_cast<uint8_t*>(name_dest),
                        reinterpret_cast<const uint8_t*>(name),
                        name_len + 1U);
            dest[10U + name_len + 1U] = is_dir ? 4U : 8U;

            ctx.offset += reclen;
            ++ctx.entry_count;
        }
    }

    return ctx.offset;
}

// -- Job control syscalls --------------------------------------------------

[[nodiscard]] uint32_t sys_setpgid(Process* process, RegisterFrame* frame) noexcept {
    return set_process_group(*process, frame->ebx, frame->ecx);
}

[[nodiscard]] uint32_t sys_getpgrp(Process* process, RegisterFrame* frame) noexcept {
    (void)frame;
    return process->pgid;
}

[[nodiscard]] uint32_t sys_getsid(Process* process, RegisterFrame* frame) noexcept {
    return query_process_session(*process, frame->ebx);
}

// -- uname syscall ---------------------------------------------------------

namespace {

struct UtsName32 {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

} // anonymous namespace

[[nodiscard]] uint32_t sys_uname(Process* process, RegisterFrame* frame) noexcept {
    UtsName32 name{};
    copy_c_string(name.sysname, sizeof(name.sysname), "XINIM");
    copy_c_string(name.nodename, sizeof(name.nodename), "xinim");
    copy_c_string(name.release, sizeof(name.release), "0.1.0");
    copy_c_string(name.version, sizeof(name.version), "XINIM i486 microkernel");
    copy_c_string(name.machine, sizeof(name.machine), "i486");
    copy_c_string(name.domainname, sizeof(name.domainname), "(none)");
    if (!write_user_bytes(process, frame->ebx, &name, static_cast<uint32_t>(sizeof(name)))) {
        return kErrnoFault;
    }
    return 0U;
}

// -- symlink/readlink syscalls ---------------------------------------------

[[nodiscard]] uint32_t sys_symlink(Process* process, RegisterFrame* frame) noexcept {
    char target[256]{};
    char linkpath[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, target, sizeof(target)) ||
        !copy_and_resolve_user_path(process, frame->ecx, linkpath, sizeof(linkpath))) {
        return kErrnoFault;
    }
    // For symlink, the target is stored as-is (not resolved)
    char raw_target[256]{};
    if (!copy_user_string(process, frame->ebx, raw_target, sizeof(raw_target))) {
        return kErrnoFault;
    }
    return ext2_reader::create_symlink_runtime(raw_target, linkpath) ? 0U : kErrnoNoSys;
}

[[nodiscard]] uint32_t sys_readlink(Process* process, RegisterFrame* frame) noexcept {
    char path[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
        return kErrnoFault;
    }
    const uint32_t bufsiz = frame->edx;
    if (bufsiz == 0U) {
        return kErrnoInvalid;
    }
    uint8_t* user_buf = nullptr;
    if (!translate_user_region(process, frame->ecx, bufsiz, &user_buf)) {
        return kErrnoFault;
    }
    char target[256]{};
    const int result = ext2_reader::readlink_runtime(path, target, sizeof(target));
    if (result < 0) {
        return kErrnoInvalid;
    }
    const uint32_t to_copy = (static_cast<uint32_t>(result) < bufsiz)
                                 ? static_cast<uint32_t>(result) : bufsiz;
    for (uint32_t i = 0U; i < to_copy; ++i) {
        user_buf[i] = static_cast<uint8_t>(target[i]);
    }
    return to_copy;
}

// -- File descriptor ops ---------------------------------------------------

[[nodiscard]] uint32_t sys_fchdir(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    (void)frame;
    // Single-directory model, always /
    return kErrnoNoSys;
}

[[nodiscard]] uint32_t sys_fchmod(Process* process, RegisterFrame* frame) noexcept {
    const int fd = resolve_fd(process, static_cast<int>(frame->ebx));
    if (fd < 0 || !bootfs::is_open(fd)) {
        return kErrnoBadF;
    }
    // For ext2 fds, update inode permissions via directory_path_for_fd or ext2_path
    // For bootfs fds, permissions are immutable (read_only/executable flags)
    // Succeed silently for non-ext2 fds (single-user model)
    return 0U;
}

[[nodiscard]] uint32_t sys_fchown(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    (void)frame;
    // Single-user model, succeed silently
    return 0U;
}

[[nodiscard]] uint32_t sys_truncate(Process* process, RegisterFrame* frame) noexcept {
    char path[256]{};
    if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
        return kErrnoFault;
    }
    // Open, truncate via bootfs, close
    const int fd = bootfs::open(path, 0x0201U, 0U); // O_WRONLY | O_TRUNC
    if (fd < 0) {
        return kErrnoNoEnt;
    }
    bootfs::close(fd);
    return 0U;
}

[[nodiscard]] uint32_t sys_ftruncate(Process* process, RegisterFrame* frame) noexcept {
    const int fd = resolve_fd(process, static_cast<int>(frame->ebx));
    if (fd < 0 || !bootfs::is_open(fd)) {
        return kErrnoBadF;
    }
    return 0U;
}

// -- Vectored I/O ----------------------------------------------------------

namespace {

struct IoVec32 {
    uint32_t base;
    uint32_t length;
};

} // anonymous namespace

[[nodiscard]] uint32_t sys_readv(Process* process, RegisterFrame* frame) noexcept {
    const int user_fd = static_cast<int>(frame->ebx);
    const int fd = resolve_fd(process, user_fd);
    const uint32_t iov_addr = frame->ecx;
    const uint32_t iov_count = frame->edx;

    if (iov_count == 0U || iov_count > 16U) {
        return kErrnoInvalid;
    }

    uint32_t total = 0U;
    for (uint32_t i = 0U; i < iov_count; ++i) {
        IoVec32 iov{};
        uint8_t* iov_raw = nullptr;
        if (!translate_user_region(process, iov_addr + i * sizeof(IoVec32),
                                   sizeof(IoVec32), &iov_raw)) {
            return kErrnoFault;
        }
        iov = *reinterpret_cast<const IoVec32*>(iov_raw);
        if (iov.length == 0U) {
            continue;
        }
        uint8_t* buf = nullptr;
        if (!translate_user_region(process, iov.base, iov.length, &buf)) {
            return kErrnoFault;
        }
        if (user_fd == 0 && (fd < 0 || !bootfs::is_open(fd))) {
            for (uint32_t j = 0U; j < iov.length; ++j) {
                char value = '\0';
                if (!console::tty_try_read_char(&value)) {
                    block_current_process_until_rescheduled(process, WaitReason::ConsoleInput, 0U);
                }
                buf[j] = static_cast<uint8_t>(value);
                ++total;
                if (value == '\r' || value == '\n') {
                    return total;
                }
            }
        } else if (fd >= 0) {
            int result = bootfs::read(fd, buf, iov.length);
            while (result == bootfs::kReadWouldBlock) {
                if (block_current_process_until_rescheduled(process, WaitReason::ConsoleInput, 0U)) {
                    return total > 0U ? total : kErrnoIntr;
                }
                result = bootfs::read(fd, buf, iov.length);
            }
            if (result < 0) {
                return total > 0U ? total : kErrnoBadF;
            }
            total += static_cast<uint32_t>(result);
            if (static_cast<uint32_t>(result) < iov.length) {
                break;
            }
        } else {
            return kErrnoBadF;
        }
    }
    return total;
}

[[nodiscard]] uint32_t sys_writev(Process* process, RegisterFrame* frame) noexcept {
    const int user_fd = static_cast<int>(frame->ebx);
    const int fd = resolve_fd(process, user_fd);
    const uint32_t iov_addr = frame->ecx;
    const uint32_t iov_count = frame->edx;

    if (iov_count == 0U || iov_count > 16U) {
        return kErrnoInvalid;
    }

    uint32_t total = 0U;
    for (uint32_t i = 0U; i < iov_count; ++i) {
        IoVec32 iov{};
        uint8_t* iov_raw = nullptr;
        if (!translate_user_region(process, iov_addr + i * sizeof(IoVec32),
                                   sizeof(IoVec32), &iov_raw)) {
            return kErrnoFault;
        }
        iov = *reinterpret_cast<const IoVec32*>(iov_raw);
        if (iov.length == 0U) {
            continue;
        }
        uint8_t* buf = nullptr;
        if (!translate_user_region(process, iov.base, iov.length, &buf)) {
            return kErrnoFault;
        }
        if ((user_fd == 1 || user_fd == 2) && (fd < 0 || !bootfs::is_open(fd))) {
            for (uint32_t j = 0U; j < iov.length; ++j) {
                console::tty_write_char(static_cast<char>(buf[j]));
            }
            total += iov.length;
        } else if (fd >= 0) {
            int result = bootfs::write(fd, buf, iov.length);
            while (result == bootfs::kWriteWouldBlock) {
                if (block_current_process_until_rescheduled(process, WaitReason::PipeIO, 0U)) {
                    return total > 0U ? total : kErrnoIntr;
                }
                result = bootfs::write(fd, buf, iov.length);
            }
            if (result < 0) {
                return total > 0U ? total : static_cast<uint32_t>(result);
            }
            total += static_cast<uint32_t>(result);
        } else {
            return kErrnoBadF;
        }
    }
    return total;
}

// -- select/poll syscalls --------------------------------------------------

namespace {

struct FdSet32 {
    uint32_t bits[32]; // 1024 bits
};

constexpr uint32_t kFdSetBits = 1024U;

[[nodiscard]] bool fd_set_is_set(const FdSet32* set, int fd) noexcept {
    if (set == nullptr || fd < 0 || static_cast<uint32_t>(fd) >= kFdSetBits) {
        return false;
    }
    return (set->bits[fd / 32] & (1U << (fd % 32))) != 0U;
}

void fd_set_set(FdSet32* set, int fd) noexcept {
    if (set == nullptr || fd < 0 || static_cast<uint32_t>(fd) >= kFdSetBits) {
        return;
    }
    set->bits[fd / 32] |= (1U << (fd % 32));
}

} // anonymous namespace

[[nodiscard]] uint32_t sys_select(Process* process, RegisterFrame* frame) noexcept {
    const int nfds = static_cast<int>(frame->ebx);
    const uint32_t readfds_addr = frame->ecx;
    const uint32_t writefds_addr = frame->edx;
    const uint32_t exceptfds_addr = frame->esi;
    const uint32_t timeout_addr = frame->edi;

    if (nfds < 0 || nfds > 64) {
        return kErrnoInvalid;
    }

    FdSet32 readfds_in{};
    FdSet32 writefds_in{};
    FdSet32 readfds_out{};
    FdSet32 writefds_out{};

    const uint32_t fdset_bytes = static_cast<uint32_t>((nfds + 7) / 8);

    if (readfds_addr != 0U) {
        uint8_t* raw = nullptr;
        if (!translate_user_region(process, readfds_addr, fdset_bytes, &raw)) {
            return kErrnoFault;
        }
        copy_region(reinterpret_cast<uint8_t*>(&readfds_in), raw, fdset_bytes);
    }
    if (writefds_addr != 0U) {
        uint8_t* raw = nullptr;
        if (!translate_user_region(process, writefds_addr, fdset_bytes, &raw)) {
            return kErrnoFault;
        }
        copy_region(reinterpret_cast<uint8_t*>(&writefds_in), raw, fdset_bytes);
    }

    // Parse timeout (NULL = block indefinitely, zero = poll)
    bool block_forever = (timeout_addr == 0U);
    uint32_t timeout_ticks = 0U;
    if (!block_forever) {
        TimeVal32 tv{};
        uint8_t* raw = nullptr;
        if (!translate_user_region(process, timeout_addr, sizeof(tv), &raw)) {
            return kErrnoFault;
        }
        tv = *reinterpret_cast<const TimeVal32*>(raw);
        // Convert to scheduler ticks (100 Hz)
        timeout_ticks = tv.seconds * kTimerHz + tv.microseconds / (1000000U / kTimerHz);
    }

    // Poll loop
    uint32_t elapsed = 0U;
    for (;;) {
        uint32_t ready_count = 0U;
        zero_region(reinterpret_cast<uint8_t*>(&readfds_out), sizeof(readfds_out));
        zero_region(reinterpret_cast<uint8_t*>(&writefds_out), sizeof(writefds_out));

        for (int user_fd = 0; user_fd < nfds; ++user_fd) {
            const int gfd = resolve_fd(process, user_fd);
            if (fd_set_is_set(&readfds_in, user_fd)) {
                if (gfd >= 0 && bootfs::is_open(gfd)) {
                    if (bootfs::is_console_fd(gfd)) {
                        if (console::tty_has_input()) {
                            fd_set_set(&readfds_out, user_fd);
                            ++ready_count;
                        }
                    } else {
                        fd_set_set(&readfds_out, user_fd);
                        ++ready_count;
                    }
                } else if (user_fd == 0) {
                    if (console::tty_has_input()) {
                        fd_set_set(&readfds_out, user_fd);
                        ++ready_count;
                    }
                }
            }
            if (fd_set_is_set(&writefds_in, user_fd)) {
                if (gfd >= 0 && bootfs::is_open(gfd)) {
                    fd_set_set(&writefds_out, user_fd);
                    ++ready_count;
                } else if (user_fd == 1 || user_fd == 2) {
                    fd_set_set(&writefds_out, user_fd);
                    ++ready_count;
                }
            }
        }

        if (ready_count > 0U || (!block_forever && elapsed >= timeout_ticks)) {
            // Write results back
            if (readfds_addr != 0U) {
                static_cast<void>(write_user_bytes(process, readfds_addr, &readfds_out, fdset_bytes));
            }
            if (writefds_addr != 0U) {
                static_cast<void>(write_user_bytes(process, writefds_addr, &writefds_out, fdset_bytes));
            }
            if (exceptfds_addr != 0U) {
                FdSet32 empty{};
                static_cast<void>(write_user_bytes(process, exceptfds_addr, &empty, fdset_bytes));
            }
            return ready_count;
        }

        // Block for one tick then re-check
        block_current_process_until_rescheduled(process, WaitReason::SleepTick,
                                                g_scheduler_ticks + 1U);
        ++elapsed;
    }
}

namespace {

struct PollFd32 {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

constexpr int16_t kPollIn = 0x0001;
constexpr int16_t kPollOut = 0x0004;
constexpr int16_t kPollNVal = 0x0020;

} // anonymous namespace

[[nodiscard]] uint32_t sys_poll(Process* process, RegisterFrame* frame) noexcept {
    const uint32_t fds_addr = frame->ebx;
    const uint32_t nfds = frame->ecx;
    const int32_t timeout_ms = static_cast<int32_t>(frame->edx);

    if (nfds > 64U) {
        return kErrnoInvalid;
    }
    if (nfds == 0U) {
        if (timeout_ms > 0) {
            const uint64_t wake = g_scheduler_ticks +
                                  static_cast<uint64_t>(timeout_ms) * kTimerHz / 1000U;
            block_current_process_until_rescheduled(process, WaitReason::SleepTick, wake);
        }
        return 0U;
    }

    uint8_t* fds_raw = nullptr;
    const uint32_t fds_size = nfds * static_cast<uint32_t>(sizeof(PollFd32));
    if (!translate_user_region(process, fds_addr, fds_size, &fds_raw)) {
        return kErrnoFault;
    }
    auto* fds = reinterpret_cast<PollFd32*>(fds_raw);

    const bool block_forever = (timeout_ms < 0);
    const uint32_t timeout_ticks = block_forever ? 0U
        : static_cast<uint32_t>(timeout_ms) * kTimerHz / 1000U;
    uint32_t elapsed = 0U;

    for (;;) {
        uint32_t ready = 0U;
        for (uint32_t i = 0U; i < nfds; ++i) {
            fds[i].revents = 0;
            const int user_fd = fds[i].fd;
            if (user_fd < 0) {
                continue;
            }
            const int gfd = resolve_fd(process, user_fd);
            const bool is_open_gfd = (gfd >= 0 && bootfs::is_open(gfd)) ||
                                     (user_fd <= 2 && gfd >= 0);
            if (!is_open_gfd) {
                fds[i].revents = kPollNVal;
                ++ready;
                continue;
            }
            if ((fds[i].events & kPollIn) != 0) {
                if (gfd >= 0 && bootfs::is_console_fd(gfd)) {
                    if (console::tty_has_input()) {
                        fds[i].revents |= kPollIn;
                    }
                } else {
                    fds[i].revents |= kPollIn;
                }
            }
            if ((fds[i].events & kPollOut) != 0) {
                fds[i].revents |= kPollOut;
            }
            if (fds[i].revents != 0) {
                ++ready;
            }
        }

        if (ready > 0U || (!block_forever && elapsed >= timeout_ticks)) {
            return ready;
        }

        block_current_process_until_rescheduled(process, WaitReason::SleepTick,
                                                g_scheduler_ticks + 1U);
        ++elapsed;
    }
}

// -- nanosleep syscall -----------------------------------------------------

[[nodiscard]] uint32_t sys_nanosleep_impl(Process* process, RegisterFrame* frame) noexcept {
    if (frame->ebx == 0U) {
        return kErrnoFault;
    }
    uint8_t* raw = nullptr;
    if (!translate_user_region(process, frame->ebx, sizeof(TimeSpec32), &raw)) {
        return kErrnoFault;
    }
    const auto* req = reinterpret_cast<const TimeSpec32*>(raw);
    const uint64_t ticks = static_cast<uint64_t>(req->seconds) * kTimerHz +
                           static_cast<uint64_t>(req->nanoseconds) / (1000000000U / kTimerHz);
    if (ticks > 0U) {
        const uint64_t target_tick = g_scheduler_ticks + ticks;
        if (block_current_process_until_rescheduled(process, WaitReason::SleepTick, target_tick)) {
            // Interrupted by signal -- write remaining time if rem provided
            if (frame->ecx != 0U) {
                const uint64_t remaining = (target_tick > g_scheduler_ticks)
                    ? (target_tick - g_scheduler_ticks) : 0U;
                const TimeSpec32 rem{
                    static_cast<uint32_t>(remaining / kTimerHz),
                    static_cast<uint32_t>((remaining % kTimerHz) * (1000000000U / kTimerHz))
                };
                static_cast<void>(write_user_bytes(process, frame->ecx, &rem,
                                                    static_cast<uint32_t>(sizeof(rem))));
            }
            return kErrnoIntr;
        }
    }
    // Write remaining time (0) if rem pointer provided
    if (frame->ecx != 0U) {
        const TimeSpec32 rem{};
        static_cast<void>(write_user_bytes(process, frame->ecx, &rem, static_cast<uint32_t>(sizeof(rem))));
    }
    return 0U;
}

// -- Syscall dispatch ------------------------------------------------------

#ifdef XINIM_X86_32_TTY_TRACE
namespace {

uint32_t g_syscall_trace_budget = 192U;

[[nodiscard]] bool trace_syscall(uint32_t number) noexcept {
    switch (number) {
    case SYS_read:
    case SYS_write:
    case SYS_open:
    case SYS_close:
    case SYS_ioctl:
    case SYS_exit:
    case SYS_execve:
    case SYS_fcntl:
    case SYS_wait4:
    case SYS_brk:
    case SYS_mmap:
    case SYS_mremap:
    case SYS_select:
    case SYS_poll:
    case SYS_readv:
    case SYS_writev:
    case SYS_set_tid_address:
        return true;
    default:
        return false;
    }
}

void trace_syscall_entry(const Process* process, const RegisterFrame* frame) noexcept {
    if (process == nullptr || frame == nullptr || g_syscall_trace_budget == 0U ||
        !trace_syscall(frame->eax)) {
        return;
    }
    --g_syscall_trace_budget;
    console::write_string("tty trace: syscall pid=");
    console::write_dec32(process->pid);
    console::write_string(" nr=");
    console::write_dec32(frame->eax);
    console::write_string(" eip=");
    console::write_hex32(process->context.eip);
    console::write_string(" esp=");
    console::write_hex32(process->context.esp);
    console::write_string(" ebx=");
    console::write_hex32(frame->ebx);
    console::write_string(" ecx=");
    console::write_hex32(frame->ecx);
    console::write_string(" edx=");
    console::write_hex32(frame->edx);
    console::newline();
}

} // namespace
#endif

[[noreturn]] void terminate_current_process_from_signal(uint32_t status) noexcept {
    exit_current_process(status, "signal terminated process", true);
}

uint32_t dispatch_syscall(Process* process, RegisterFrame* frame) noexcept {
    process->context = capture_user_context(frame);
#ifdef XINIM_X86_32_TTY_TRACE
    trace_syscall_entry(process, frame);
#endif

    switch (frame->eax) {
    case SYS_read:
        return sys_read(process, frame);
    case SYS_write:
        return sys_write(process, frame);
    case SYS_open:
        return sys_open(process, frame);
    case SYS_close: {
        const int user_fd = static_cast<int>(frame->ebx);
        if (user_fd < 0 || user_fd >= kMaxFds) {
            return kErrnoBadF;
        }
        const int global_slot = process->fd_map[user_fd];
        if (global_slot < 0) {
            return kErrnoBadF;
        }
        process->fd_map[user_fd] = -1;
        process->fd_flags[user_fd] = 0;
        bootfs::decrement_slot_refcount(global_slot);
        return 0U;
    }
    case SYS_access:
        return sys_access(process, frame);
    case SYS_mkdir:
        return sys_mkdir(process, frame);
    case SYS_rmdir:
        return sys_rmdir(process, frame);
    case SYS_rename:
        return sys_rename(process, frame);
    case SYS_unlink:
        return sys_unlink(process, frame);
    case SYS_chdir:
        return sys_chdir(process, frame);
    case SYS_getcwd:
        return sys_getcwd(process, frame);
    case SYS_link: {
        char oldpath[256]{};
        char newpath[256]{};
        if (!copy_and_resolve_user_path(process, frame->ebx, oldpath, sizeof(oldpath)) ||
            !copy_and_resolve_user_path(process, frame->ecx, newpath, sizeof(newpath))) {
            return kErrnoFault;
        }
        return ext2_reader::link_runtime_file(oldpath, newpath) ? 0U : kErrnoNoSys;
    }
    case SYS_chmod: {
        char path[256]{};
        if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
            return kErrnoFault;
        }
        ext2_reader::NodeInfo ext2_info{};
        if (!ext2_reader::query_runtime_path(path, ext2_info)) {
            return kErrnoNoEnt;
        }
        if (!ext2_reader::chmod_runtime_file(path, static_cast<uint16_t>(frame->ecx))) {
            return kErrnoNoEnt;
        }
        return 0U;
    }
    case SYS_chown:
        return 0U; // Single-user model, succeed silently
    case SYS_setuid: {
        const uint32_t new_uid = frame->ebx;
        if (process->cred.euid == 0U) {
            // Superuser: set all three
            process->cred.uid = new_uid;
            process->cred.euid = new_uid;
            process->cred.suid = new_uid;
        } else if (new_uid == process->cred.uid || new_uid == process->cred.suid) {
            process->cred.euid = new_uid;
        } else {
            return kErrnoPerm;
        }
        return 0U;
    }
    case SYS_setgid: {
        const uint32_t new_gid = frame->ebx;
        if (process->cred.euid == 0U) {
            process->cred.gid = new_gid;
            process->cred.egid = new_gid;
            process->cred.sgid = new_gid;
        } else if (new_gid == process->cred.gid || new_gid == process->cred.sgid) {
            process->cred.egid = new_gid;
        } else {
            return kErrnoPerm;
        }
        return 0U;
    }
    case SYS_exit:
        exit_current_process(frame->ebx, "orphaned i486 user process exited");
    case SYS_getpid:
        return process->pid;
    case SYS_getppid:
        return process->ppid;
    case SYS_fork:
        return sys_fork(process, frame);
    case SYS_execve:
        sys_execve(process, frame);
    case SYS_wait4:
        return sys_wait4(process, frame);
    case SYS_lseek:
        return sys_lseek(process, frame);
    case SYS_stat:
        return sys_stat(process, frame);
    case SYS_fstat:
        return sys_fstat(process, frame);
    case SYS_dup:
        return sys_dup(process, frame);
    case SYS_dup2:
        return sys_dup2(process, frame);
    case SYS_pipe:
        return sys_pipe(process, frame);
    case SYS_ioctl:
        return sys_ioctl(process, frame);
    case SYS_fcntl:
        return sys_fcntl(process, frame);
    case SYS_brk:
        return sys_brk(process, frame);
    case SYS_mmap:
        return sys_mmap(process, frame);
    case SYS_munmap:
        return sys_munmap(process, frame);
    case SYS_mprotect:
        return sys_mprotect_compat(process, frame);
    case SYS_kill:
        return sys_kill_impl(process, frame);
    case SYS_signal:
        return sys_signal_impl(process, frame);
    case SYS_sigaction:
        return sys_sigaction_impl(process, frame);
    case SYS_time:
        return sys_time_compat(process, frame);
    case SYS_gettimeofday:
        return sys_gettimeofday_compat(process, frame);
    case SYS_clock_gettime:
        return sys_clock_gettime_compat(process, frame);
    case SYS_nanosleep:
        return sys_nanosleep_impl(process, frame);
    case SYS_getuid:
        return process->cred.uid;
    case SYS_geteuid:
        return process->cred.euid;
    case SYS_getgid:
        return process->cred.gid;
    case SYS_getegid:
        return process->cred.egid;
    case SYS_alarm:
        return sys_alarm_compat(process, frame);
    case SYS_getrlimit:
        return sys_getrlimit_compat(process, frame);
    case SYS_setrlimit:
        return sys_setrlimit_compat(process, frame);
    case SYS_getrusage:
        return sys_getrusage_compat(process, frame);
    case SYS_setsid:
        return sys_setsid_compat(process, frame);
    case SYS_umask:
        return sys_umask_compat(process, frame);
    case SYS_nice:
        return sys_nice_compat(process, frame);
    case SYS_setreuid: {
        const uint32_t ruid = frame->ebx;
        const uint32_t euid = frame->ecx;
        if (ruid != 0xFFFFFFFFU) {
            if (process->cred.euid == 0U || ruid == process->cred.uid ||
                ruid == process->cred.euid) {
                process->cred.uid = ruid;
            } else {
                return kErrnoPerm;
            }
        }
        if (euid != 0xFFFFFFFFU) {
            if (process->cred.euid == 0U || euid == process->cred.uid ||
                euid == process->cred.euid || euid == process->cred.suid) {
                process->cred.euid = euid;
            } else {
                return kErrnoPerm;
            }
        }
        if (ruid != 0xFFFFFFFFU || (euid != 0xFFFFFFFFU && euid != process->cred.uid)) {
            process->cred.suid = process->cred.euid;
        }
        return 0U;
    }
    case SYS_setregid: {
        const uint32_t rgid = frame->ebx;
        const uint32_t egid = frame->ecx;
        if (rgid != 0xFFFFFFFFU) {
            if (process->cred.euid == 0U || rgid == process->cred.gid ||
                rgid == process->cred.egid) {
                process->cred.gid = rgid;
            } else {
                return kErrnoPerm;
            }
        }
        if (egid != 0xFFFFFFFFU) {
            if (process->cred.euid == 0U || egid == process->cred.gid ||
                egid == process->cred.egid || egid == process->cred.sgid) {
                process->cred.egid = egid;
            } else {
                return kErrnoPerm;
            }
        }
        if (rgid != 0xFFFFFFFFU || (egid != 0xFFFFFFFFU && egid != process->cred.gid)) {
            process->cred.sgid = process->cred.egid;
        }
        return 0U;
    }
    case SYS_rt_sigprocmask:
        return sys_rt_sigprocmask_compat(process, frame);
    case SYS_getpgid: {
        // getpgid(0) returns caller's pgid; getpgid(pid) returns that process's pgid
        const uint32_t target = frame->ebx;
        if (target == 0U) {
            return process->pgid;
        }
        Process* target_proc = find_process(target);
        return target_proc != nullptr ? target_proc->pgid : kErrnoSrch;
    }
    case SYS_getdents:
        return sys_getdents(process, frame);
    case SYS_mremap:
        return sys_mremap(process, frame);
    // Phase 2 syscalls
    case SYS_setpgid:
        return sys_setpgid(process, frame);
    case SYS_getpgrp:
        return sys_getpgrp(process, frame);
    case SYS_getsid:
        return sys_getsid(process, frame);
    case SYS_uname:
        return sys_uname(process, frame);
    case SYS_symlink:
        return sys_symlink(process, frame);
    case SYS_readlink:
        return sys_readlink(process, frame);
    case SYS_fchdir:
        return sys_fchdir(process, frame);
    case SYS_fchmod:
        return sys_fchmod(process, frame);
    case SYS_fchown:
        return sys_fchown(process, frame);
    case SYS_truncate:
        return sys_truncate(process, frame);
    case SYS_ftruncate:
        return sys_ftruncate(process, frame);
    case SYS_readv:
        return sys_readv(process, frame);
    case SYS_writev:
        return sys_writev(process, frame);
    case SYS_select:
        return sys_select(process, frame);
    case SYS_poll:
        return sys_poll(process, frame);
    case SYS_rt_sigreturn:
        sys_rt_sigreturn_impl(process, frame);
    // Phase 4 socket syscalls
    case SYS_socket:
        return static_cast<uint32_t>(ksocket::sys_socket(
            static_cast<int>(frame->ebx),
            static_cast<int>(frame->ecx),
            static_cast<int>(frame->edx)));
    case SYS_bind: {
        uint8_t* addr_raw = nullptr;
        if (frame->ecx != 0U && !translate_user_region(process, frame->ecx, 16U, &addr_raw)) {
            return kErrnoFault;
        }
        return static_cast<uint32_t>(ksocket::sys_bind(
            static_cast<int>(frame->ebx),
            reinterpret_cast<const ksocket::SockAddrIn*>(addr_raw)));
    }
    case SYS_connect: {
        uint8_t* addr_raw = nullptr;
        if (frame->ecx != 0U && !translate_user_region(process, frame->ecx, 16U, &addr_raw)) {
            return kErrnoFault;
        }
        int rc = ksocket::sys_connect(
            static_cast<int>(frame->ebx),
            reinterpret_cast<const ksocket::SockAddrIn*>(addr_raw));
        if (rc == -115) {
            // EINPROGRESS: block until TCP state changes
            const int sockfd = static_cast<int>(frame->ebx);
            process->wait_tcp_conn = ksocket::get_tcp_conn_idx(sockfd);
            block_current_process_until_rescheduled(process, WaitReason::TcpConnect, 0U);
            // On wake, retry the connect call (socket remembers SynSent state)
            rc = ksocket::sys_connect(sockfd,
                reinterpret_cast<const ksocket::SockAddrIn*>(addr_raw));
            if (rc == -115) {
                rc = -110; // ETIMEDOUT if still not connected
            }
        }
        return static_cast<uint32_t>(rc);
    }
    case SYS_listen:
        return static_cast<uint32_t>(ksocket::sys_listen(
            static_cast<int>(frame->ebx),
            static_cast<int>(frame->ecx)));
    case SYS_accept:
        return static_cast<uint32_t>(ksocket::sys_accept(
            static_cast<int>(frame->ebx), nullptr));
    case SYS_sendto: {
        uint8_t* buf_raw = nullptr;
        if (!translate_user_region(process, frame->ecx, frame->edx, &buf_raw)) {
            return kErrnoFault;
        }
        uint8_t* addr_raw = nullptr;
        if (frame->edi != 0U) {
            static_cast<void>(translate_user_region(process, frame->edi, 16U, &addr_raw));
        }
        return static_cast<uint32_t>(ksocket::sys_sendto(
            static_cast<int>(frame->ebx), buf_raw, frame->edx,
            reinterpret_cast<const ksocket::SockAddrIn*>(addr_raw)));
    }
    case SYS_recvfrom: {
        uint8_t* buf_raw = nullptr;
        if (!translate_user_region(process, frame->ecx, frame->edx, &buf_raw)) {
            return kErrnoFault;
        }
        return static_cast<uint32_t>(ksocket::sys_recvfrom(
            static_cast<int>(frame->ebx), buf_raw, frame->edx, nullptr));
    }
    case SYS_shutdown:
        return static_cast<uint32_t>(ksocket::sys_shutdown(
            static_cast<int>(frame->ebx),
            static_cast<int>(frame->ecx)));
    case SYS_setsockopt:
        return static_cast<uint32_t>(ksocket::sys_setsockopt(
            static_cast<int>(frame->ebx),
            static_cast<int>(frame->ecx),
            static_cast<int>(frame->edx),
            reinterpret_cast<const void*>(frame->esi),
            frame->edi));
    case SYS_getsockopt:
        return static_cast<uint32_t>(ksocket::sys_getsockopt(
            static_cast<int>(frame->ebx),
            static_cast<int>(frame->ecx),
            static_cast<int>(frame->edx),
            reinterpret_cast<void*>(frame->esi),
            reinterpret_cast<uint32_t*>(frame->edi)));
    case SYS_getsockname:
        return static_cast<uint32_t>(ksocket::sys_getsockname(
            static_cast<int>(frame->ebx),
            reinterpret_cast<ksocket::SockAddrIn*>(frame->ecx)));
    case SYS_getpeername:
        return static_cast<uint32_t>(ksocket::sys_getpeername(
            static_cast<int>(frame->ebx),
            reinterpret_cast<ksocket::SockAddrIn*>(frame->ecx)));
    case SYS_socketpair:
        return static_cast<uint32_t>(ksocket::sys_socketpair(
            static_cast<int>(frame->ebx),
            static_cast<int>(frame->ecx),
            static_cast<int>(frame->edx),
            reinterpret_cast<int*>(frame->esi)));
    case SYS_sendmsg: {
        // ebx = sockfd, ecx = msghdr* (userspace), edx = flags (ignored)
        uint8_t* msg_raw = nullptr;
        if (frame->ecx == 0U || !translate_user_region(process, frame->ecx,
                static_cast<uint32_t>(sizeof(ksocket::MsgHdr32)), &msg_raw)) {
            return kErrnoFault;
        }
        const auto* hdr = reinterpret_cast<const ksocket::MsgHdr32*>(msg_raw);
        // Gather iov into a single buffer
        if (hdr->msg_iovlen == 0U) {
            return 0U;
        }
        uint8_t gather_buf[4096]{};
        uint32_t total = 0U;
        for (uint32_t i = 0U; i < hdr->msg_iovlen && total < sizeof(gather_buf); ++i) {
            uint8_t* iov_raw = nullptr;
            const uint32_t iov_addr = hdr->msg_iov + i * 8U;
            if (!translate_user_region(process, iov_addr, 8U, &iov_raw)) {
                break;
            }
            const auto* iov = reinterpret_cast<const ksocket::IoVec32*>(iov_raw);
            uint32_t len = iov->iov_len;
            if (total + len > sizeof(gather_buf)) {
                len = static_cast<uint32_t>(sizeof(gather_buf)) - total;
            }
            uint8_t* data_raw = nullptr;
            if (iov->iov_base != 0U && len > 0U &&
                translate_user_region(process, iov->iov_base, len, &data_raw)) {
                for (uint32_t j = 0U; j < len; ++j) {
                    gather_buf[total + j] = data_raw[j];
                }
                total += len;
            }
        }
        // Resolve optional destination address
        const ksocket::SockAddrIn* dest = nullptr;
        uint8_t* name_raw = nullptr;
        if (hdr->msg_name != 0U && hdr->msg_namelen >= sizeof(ksocket::SockAddrIn) &&
            translate_user_region(process, hdr->msg_name,
                static_cast<uint32_t>(sizeof(ksocket::SockAddrIn)), &name_raw)) {
            dest = reinterpret_cast<const ksocket::SockAddrIn*>(name_raw);
        }
        return static_cast<uint32_t>(ksocket::sys_sendto(
            static_cast<int>(frame->ebx), gather_buf, total, dest));
    }
    case SYS_recvmsg: {
        // ebx = sockfd, ecx = msghdr* (userspace), edx = flags (ignored)
        uint8_t* msg_raw = nullptr;
        if (frame->ecx == 0U || !translate_user_region(process, frame->ecx,
                static_cast<uint32_t>(sizeof(ksocket::MsgHdr32)), &msg_raw)) {
            return kErrnoFault;
        }
        auto* hdr = reinterpret_cast<ksocket::MsgHdr32*>(msg_raw);
        if (hdr->msg_iovlen == 0U) {
            return 0U;
        }
        // Compute total iov capacity
        uint32_t total_cap = 0U;
        for (uint32_t i = 0U; i < hdr->msg_iovlen; ++i) {
            uint8_t* iov_raw = nullptr;
            const uint32_t iov_addr = hdr->msg_iov + i * 8U;
            if (!translate_user_region(process, iov_addr, 8U, &iov_raw)) {
                break;
            }
            const auto* iov = reinterpret_cast<const ksocket::IoVec32*>(iov_raw);
            total_cap += iov->iov_len;
        }
        if (total_cap > 4096U) {
            total_cap = 4096U;
        }
        // Receive into scratch buffer
        uint8_t recv_buf[4096]{};
        const int rc = ksocket::sys_recvfrom(
            static_cast<int>(frame->ebx), recv_buf, total_cap, nullptr);
        if (rc <= 0) {
            return static_cast<uint32_t>(rc);
        }
        // Scatter into iov
        uint32_t remaining = static_cast<uint32_t>(rc);
        uint32_t offset = 0U;
        for (uint32_t i = 0U; i < hdr->msg_iovlen && remaining > 0U; ++i) {
            uint8_t* iov_raw = nullptr;
            const uint32_t iov_addr = hdr->msg_iov + i * 8U;
            if (!translate_user_region(process, iov_addr, 8U, &iov_raw)) {
                break;
            }
            const auto* iov = reinterpret_cast<const ksocket::IoVec32*>(iov_raw);
            uint32_t chunk = iov->iov_len;
            if (chunk > remaining) {
                chunk = remaining;
            }
            if (iov->iov_base != 0U && chunk > 0U) {
                static_cast<void>(write_user_bytes(process, iov->iov_base, recv_buf + offset, chunk));
            }
            offset += chunk;
            remaining -= chunk;
        }
        hdr->msg_flags = 0;
        return static_cast<uint32_t>(rc);
    }
    // Phase 3 POSIX completeness syscalls
    case SYS_gettid:
        return process->pid; // No threads, tid == pid
    case SYS_sched_yield:
        process->ticks_remaining = 0U;
        return 0U;
    case SYS_lstat: {
        char path[256]{};
        if (!copy_and_resolve_user_path(process, frame->ebx, path, sizeof(path))) {
            return kErrnoFault;
        }
        // Try ext2 lstat (no-follow) first
        ext2_reader::NodeInfo info{};
        if (ext2_reader::query_runtime_path_no_follow(path, info) && info.exists) {
            bootfs::UserspaceStat host_stat{};
            if (bootfs::stat_path(path, &host_stat) != 0) {
                return kErrnoNoEnt;
            }
            // Override mode with no-follow mode (preserves symlink type bit)
            host_stat.st_mode = info.mode;
            host_stat.st_size = info.size;
            uint8_t* destination = nullptr;
            if (!translate_user_region(process, frame->ecx, sizeof(host_stat), &destination)) {
                return kErrnoFault;
            }
            *reinterpret_cast<bootfs::UserspaceStat*>(destination) = host_stat;
            return 0U;
        }
        // Fall back to regular stat for non-ext2 paths
        return sys_stat(process, frame);
    }
    case SYS_dup3: {
        const int old_user_fd = static_cast<int>(frame->ebx);
        const int new_user_fd = static_cast<int>(frame->ecx);
        const uint32_t flags = frame->edx;
        if (new_user_fd < 0 || new_user_fd >= kMaxFds) {
            return kErrnoInvalid;
        }
        const int global_slot = resolve_fd(process, old_user_fd);
        if (global_slot < 0 || !bootfs::is_open(global_slot)) {
            return kErrnoBadF;
        }
        if (old_user_fd == new_user_fd) {
            return kErrnoInvalid; // dup3 requires old != new
        }
        const int existing = process->fd_map[new_user_fd];
        if (existing >= 0) {
            bootfs::decrement_slot_refcount(existing);
        }
        process->fd_map[new_user_fd] = global_slot;
        bootfs::increment_slot_refcount(global_slot);
        process->fd_flags[new_user_fd] = ((flags & 0x80000U) != 0U) ? 1 : 0;
        return static_cast<uint32_t>(new_user_fd);
    }
    case SYS_pipe2: {
        int pipe_fds[2] = {-1, -1};
        const int result = bootfs::make_pipe(pipe_fds);
        if (result != 0) {
            return kErrnoNoMem;
        }
        const uint32_t flags = frame->ecx;
        const int cloexec = ((flags & 0x80000U) != 0U) ? 1 : 0;
        const int local_read = allocate_fd_map_entry(process, pipe_fds[0]);
        const int local_write = allocate_fd_map_entry(process, pipe_fds[1]);
        if (local_read < 0 || local_write < 0) {
            if (local_read >= 0) {
                process->fd_map[local_read] = -1;
            }
            bootfs::close(pipe_fds[0]);
            bootfs::close(pipe_fds[1]);
            return kErrnoNoMem;
        }
        if (!write_user_u32(process, frame->ebx, static_cast<uint32_t>(local_read)) ||
            !write_user_u32(process, frame->ebx + sizeof(uint32_t), static_cast<uint32_t>(local_write))) {
            process->fd_map[local_read] = -1;
            process->fd_map[local_write] = -1;
            bootfs::close(pipe_fds[0]);
            bootfs::close(pipe_fds[1]);
            return kErrnoFault;
        }
        process->fd_flags[local_read] = cloexec;
        process->fd_flags[local_write] = cloexec;
        return 0U;
    }
    case SYS_fsync:
    case SYS_fdatasync: {
        const int gfd = resolve_fd(process, static_cast<int>(frame->ebx));
        return (gfd >= 0 && bootfs::is_open(gfd)) ? 0U : kErrnoBadF;
    }
    case SYS_flock: {
        const int gfd = resolve_fd(process, static_cast<int>(frame->ebx));
        if (gfd < 0 || !bootfs::is_open(gfd)) {
            return kErrnoBadF;
        }
        return static_cast<uint32_t>(
            lockf::do_flock(gfd, static_cast<int>(frame->ecx), process->pid));
    }
    case SYS_set_tid_address:
        // Store clear_child_tid pointer (simplified: just return pid)
        return process->pid;
    case SYS_statfs: {
        // Return basic filesystem info
        struct StatFs32 {
            uint32_t f_type;
            uint32_t f_bsize;
            uint32_t f_blocks;
            uint32_t f_bfree;
            uint32_t f_bavail;
            uint32_t f_files;
            uint32_t f_ffree;
            uint32_t f_fsid[2];
            uint32_t f_namelen;
            uint32_t f_frsize;
            uint32_t f_flags;
            uint32_t f_spare[4];
        };
        StatFs32 fs{};
        fs.f_type = 0xEF53U; // EXT2_SUPER_MAGIC
        fs.f_bsize = 1024U;
        fs.f_blocks = 15360U;
        fs.f_bfree = 10000U;
        fs.f_bavail = 10000U;
        fs.f_files = 256U;
        fs.f_ffree = 200U;
        fs.f_namelen = 255U;
        fs.f_frsize = 1024U;
        if (!write_user_bytes(process, frame->ecx, &fs, static_cast<uint32_t>(sizeof(fs)))) {
            return kErrnoFault;
        }
        return 0U;
    }
    case SYS_openat: {
        // AT_FDCWD (-100) means use cwd; otherwise relative to dirfd
        // Simplified: ignore dirfd, treat path from ecx as absolute
        char path[256]{};
        if (!copy_and_resolve_user_path(process, frame->ecx, path, sizeof(path))) {
            return kErrnoFault;
        }
        const int global_slot = bootfs::open(path, frame->edx, frame->esi);
        if (global_slot < 0) {
            return static_cast<uint32_t>(global_slot);
        }
        const int local_fd = allocate_fd_map_entry(process, global_slot);
        if (local_fd < 0) {
            bootfs::close(global_slot);
            return kErrnoNoMem;
        }
        return static_cast<uint32_t>(local_fd);
    }
    case SYS_mkdirat:
        return sys_mkdir(process, frame);
    case SYS_unlinkat:
        return sys_unlink(process, frame);
    case SYS_getdents64:
        return sys_getdents(process, frame); // Same implementation, struct compat
    case SYS_sigpending: {
        const uint32_t pending = process->signals.pending & ~process->signals.blocked;
        if (frame->ebx != 0U && !write_user_u32(process, frame->ebx, pending)) {
            return kErrnoFault;
        }
        return 0U;
    }
    case SYS_sigsuspend: {
        // Atomically replace signal mask and suspend until a signal arrives
        const uint32_t old_mask = process->signals.blocked;
        if (frame->ebx != 0U) {
            uint32_t new_mask = 0U;
            if (!read_user_u32(process, frame->ebx, &new_mask)) {
                return kErrnoFault;
            }
            process->signals.blocked = new_mask;
        }
        // Block until any unblocked signal is pending (even SIG_DFL ones like SIGCHLD)
        for (;;) {
            const uint32_t deliverable =
                process->signals.pending & ~process->signals.blocked;
            if (deliverable != 0U) {
                // Clear SIG_DFL-ignore signals (like SIGCHLD) from pending
                // but still wake -- the process needs to call wait4
                for (uint32_t sig = 1U; sig < kMaxSignals; ++sig) {
                    if ((deliverable & (1U << sig)) != 0U &&
                        process->signals.handlers[sig].handler == kSigDfl &&
                        is_default_ignore(sig)) {
                        process->signals.pending &= ~(1U << sig);
                    }
                }
                break;
            }
            block_current_process_until_rescheduled(process, WaitReason::PipeIO, 0U);
        }
        process->signals.blocked = old_mask;
        return kErrnoIntr;
    }
    case SYS_procinfo: {
        // Dump process table to user buffer
        // ebx = user buffer, ecx = buffer size
        // Each entry: pid(4), ppid(4), pgid(4), state(1), pad(3) = 16 bytes
        struct ProcInfoEntry {
            uint32_t pid;
            uint32_t ppid;
            uint32_t pgid;
            uint8_t state; // 0=empty, 1=runnable, 2=waiting, 3=exited
            uint8_t pad[3];
        };
        const uint32_t buf_addr = frame->ebx;
        const uint32_t buf_size = frame->ecx;
        if (buf_addr == 0U || buf_size < sizeof(ProcInfoEntry)) {
            return kErrnoInvalid;
        }
        uint32_t offset = 0U;
        uint32_t count = 0U;
        for (const auto& proc : g_processes) {
            if (!proc.in_use) {
                continue;
            }
            if (offset + sizeof(ProcInfoEntry) > buf_size) {
                break;
            }
            ProcInfoEntry entry{};
            entry.pid = proc.pid;
            entry.ppid = proc.ppid;
            entry.pgid = proc.pgid;
            entry.state = static_cast<uint8_t>(proc.state);
            if (!write_user_bytes(process, buf_addr + offset, &entry,
                                  static_cast<uint32_t>(sizeof(entry)))) {
                break;
            }
            offset += static_cast<uint32_t>(sizeof(entry));
            ++count;
        }
        return count;
    }
    case SYS_setitimer:
        return sys_setitimer_impl(process, frame);
    case SYS_getitimer:
        return sys_getitimer_impl(process, frame);
    case SYS_getgroups: {
        const uint32_t gidsetsize = frame->ebx;
        if (gidsetsize == 0U) {
            return process->cred.ngroups;
        }
        if (gidsetsize < process->cred.ngroups) {
            return kErrnoInvalid;
        }
        if (frame->ecx == 0U) {
            return kErrnoFault;
        }
        if (!write_user_bytes(process, frame->ecx, process->cred.groups,
                              process->cred.ngroups * 4U)) {
            return kErrnoFault;
        }
        return process->cred.ngroups;
    }
    case SYS_setgroups: {
        const uint32_t ngroups = frame->ebx;
        if (process->cred.euid != 0U) {
            return kErrnoPerm;
        }
        if (ngroups > kMaxGroups) {
            return kErrnoInvalid;
        }
        if (ngroups > 0U) {
            uint8_t* raw = nullptr;
            if (frame->ecx == 0U || !translate_user_region(process, frame->ecx,
                    ngroups * 4U, &raw)) {
                return kErrnoFault;
            }
            for (uint32_t i = 0U; i < ngroups; ++i) {
                process->cred.groups[i] = reinterpret_cast<const uint32_t*>(raw)[i];
            }
        }
        process->cred.ngroups = ngroups;
        return 0U;
    }

    // -- SysV IPC syscalls ------------------------------------------------
    case SYS_shmget:
        return static_cast<uint32_t>(ipc::sys_shmget(
            static_cast<ipc::key_t>(frame->ebx),
            frame->ecx,
            static_cast<int>(frame->edx)));
    case SYS_shmat: {
        uint32_t result_addr = 0U;
        const int rc = ipc::sys_shmat(
            static_cast<int>(frame->ebx), frame->ecx,
            static_cast<int>(frame->edx), &result_addr);
        if (rc < 0) {
            return static_cast<uint32_t>(rc);
        }
        return result_addr;
    }
    case SYS_shmdt:
        return static_cast<uint32_t>(ipc::sys_shmdt(frame->ebx));
    case SYS_shmctl: {
        ipc::ShmIdDs* buf = nullptr;
        if (frame->edx != 0U) {
            uint8_t* raw = nullptr;
            if (!translate_user_region(process, frame->edx,
                    static_cast<uint32_t>(sizeof(ipc::ShmIdDs)), &raw)) {
                return kErrnoFault;
            }
            buf = reinterpret_cast<ipc::ShmIdDs*>(raw);
        }
        return static_cast<uint32_t>(ipc::sys_shmctl(
            static_cast<int>(frame->ebx),
            static_cast<int>(frame->ecx), buf));
    }
    case SYS_semget:
        return static_cast<uint32_t>(ipc::sys_semget(
            static_cast<ipc::key_t>(frame->ebx),
            static_cast<int>(frame->ecx),
            static_cast<int>(frame->edx)));
    case SYS_semop: {
        uint8_t* raw = nullptr;
        const uint32_t nsops = frame->edx;
        if (frame->ecx == 0U || nsops == 0U) {
            return kErrnoInvalid;
        }
        if (!translate_user_region(process, frame->ecx,
                nsops * static_cast<uint32_t>(sizeof(ipc::SemBuf)), &raw)) {
            return kErrnoFault;
        }
        return static_cast<uint32_t>(ipc::sys_semop(
            static_cast<int>(frame->ebx),
            reinterpret_cast<const ipc::SemBuf*>(raw), nsops));
    }
    case SYS_semctl:
        return static_cast<uint32_t>(ipc::sys_semctl(
            static_cast<int>(frame->ebx),
            static_cast<int>(frame->ecx),
            static_cast<int>(frame->edx),
            static_cast<int>(frame->esi)));
    case SYS_msgget:
        return static_cast<uint32_t>(ipc::sys_msgget(
            static_cast<ipc::key_t>(frame->ebx),
            static_cast<int>(frame->ecx)));
    case SYS_msgsnd: {
        uint8_t* raw = nullptr;
        const uint32_t msgsz = frame->edx;
        if (frame->ecx == 0U) {
            return kErrnoFault;
        }
        if (!translate_user_region(process, frame->ecx, 4U + msgsz, &raw)) {
            return kErrnoFault;
        }
        return static_cast<uint32_t>(ipc::sys_msgsnd(
            static_cast<int>(frame->ebx), raw, msgsz,
            static_cast<int>(frame->esi)));
    }
    case SYS_msgrcv: {
        uint8_t* raw = nullptr;
        const uint32_t msgsz = frame->edx;
        if (frame->ecx == 0U) {
            return kErrnoFault;
        }
        if (!translate_user_region(process, frame->ecx, 4U + msgsz, &raw)) {
            return kErrnoFault;
        }
        return static_cast<uint32_t>(ipc::sys_msgrcv(
            static_cast<int>(frame->ebx), raw, msgsz,
            static_cast<int32_t>(frame->esi),
            static_cast<int>(frame->edi)));
    }
    case SYS_msgctl: {
        ipc::MsgIdDs* buf = nullptr;
        if (frame->edx != 0U) {
            uint8_t* raw = nullptr;
            if (!translate_user_region(process, frame->edx,
                    static_cast<uint32_t>(sizeof(ipc::MsgIdDs)), &raw)) {
                return kErrnoFault;
            }
            buf = reinterpret_cast<ipc::MsgIdDs*>(raw);
        }
        return static_cast<uint32_t>(ipc::sys_msgctl(
            static_cast<int>(frame->ebx),
            static_cast<int>(frame->ecx), buf));
    }

    default:
        uint32_t caller = 0U;
        const bool have_caller = read_user_u32(process, process->context.esp, &caller);
        console::write_string("Unhandled i486 syscall eax=");
        console::write_dec32(frame->eax);
        console::write_string(" eip=");
        console::write_hex32(process->context.eip);
        console::write_string(" esp=");
        console::write_hex32(process->context.esp);
        console::write_string(" caller=");
        console::write_hex32(have_caller ? caller : 0U);
        console::write_string(" ebx=");
        console::write_hex32(frame->ebx);
        console::write_string(" ecx=");
        console::write_hex32(frame->ecx);
        console::write_string(" edx=");
        console::write_hex32(frame->edx);
        console::newline();
        return kErrnoNoSys;
    }
}

extern "C" uint32_t i486_handle_syscall(RegisterFrame* frame) noexcept {
    Process* process = g_current_process;
    if (frame == nullptr || process == nullptr) {
        return static_cast<uint32_t>(-1);
    }
    const uint32_t result = dispatch_syscall(process, frame);
    return complete_syscall_return(process, frame, result);
}

// -- Fault handling --------------------------------------------------------

[[noreturn]] void handle_fault(uint32_t vector, uint32_t error_code,
                              uint32_t fault_eip) noexcept {
    Process* process = g_current_process;

    console::write_string("i486 fault vector=");
    console::write_dec32(vector);
    console::write_string(" error=");
    console::write_hex32(error_code);
    if (process != nullptr) {
        console::write_string(" pid=");
        console::write_dec32(process->pid);
    }
    console::write_string(" eip=");
    console::write_hex32(fault_eip);
    if (process != nullptr) {
        console::write_string(" ctx.eip=");
        console::write_hex32(process->context.eip);
        console::write_string(" esp=");
        console::write_hex32(process->context.esp);
    }

    if (vector == 14U) {
        uint32_t fault_address = 0U;
        asm volatile("mov %%cr2, %0" : "=r"(fault_address));
        console::write_string(" cr2=");
        console::write_hex32(fault_address);
        console::newline();

        if (process != nullptr) {
            const uint32_t user_base = elf32::kUserVirtualBase;
            const uint32_t user_top = user_base + elf32::kUserAddressSpaceSize;
            const uint32_t stack_bottom = process->context.esp > kHeapGuardBytes
                                              ? process->context.esp - kHeapGuardBytes
                                              : user_base;

            if (fault_address < user_base || fault_address >= user_top) {
                console::write_string("  cause: access outside user address space");
            } else if (fault_address < process->minimum_break) {
                console::write_string("  cause: access below program text/data");
            } else if (fault_address >= process->current_break &&
                       fault_address < stack_bottom) {
                console::write_string("  cause: access in heap guard region (heap overflow)");
            } else if (fault_address < process->context.esp &&
                       fault_address >= align_down(stack_bottom, kPageSize)) {
                console::write_string("  cause: stack overflow");
            } else {
                console::write_string("  cause: invalid memory access");
            }
            console::write_string(" brk=");
            console::write_hex32(process->current_break);
            console::newline();
        }
    } else {
        console::newline();
    }

    if (process != nullptr) {
        if (find_supervised_service_by_process(process) != nullptr) {
            exit_current_process(128U + vector, "supervised i486 service faulted", true);
        }
        exit_current_process(128U + vector, "child user process faulted", true);
    }

    resume_rescue_shell("Ring 3 shell faulted");
}

extern "C" [[noreturn]] void i486_handle_fault(uint32_t vector,
                                                uint32_t error_code,
                                                uint32_t fault_eip) noexcept {
    handle_fault(vector, error_code, fault_eip);
}

// -- Init shell launch -----------------------------------------------------

bool launch_init_shell(const xinim::boot::BootInfo& info) noexcept {
    g_boot_info = &info;

    const char* shell_path = nullptr;
    const char* shell_env = nullptr;
    const bootfs::FileRecord* shell = select_init_shell(&shell_path, &shell_env);
    if (shell == nullptr || !shell->executable) {
        return false;
    }

    console::write_string("Seeding supervised init service from Multiboot2 module");
    console::newline();

    initialize_protection();

    Process* shell_process = allocate_process(0U);
    if (shell_process == nullptr) {
        return false;
    }
    SupervisedService* init_service = register_supervised_service(
        "init-shell",
        shell,
        shell_path,
        shell_env,
        xinim::kernel::recovery::RestartPolicy::RESTART,
        kInitServiceMaxRestarts,
        true,
        shell_process,
        ServiceLaunchMode::BootOnly,
        kInitServicePriority,
        0U,
        1U);
    if (init_service == nullptr) {
        destroy_process(shell_process);
        return false;
    }
    if (!prepare_supervised_service_process(init_service, shell_process)) {
        destroy_process(shell_process);
        set_service_state(init_service, xinim::kernel::recovery::ServiceState::CRASHED);
        return false;
    }

    Process* hold_process = nullptr;
    SupervisedService* hold_service = register_optional_support_services(init_service, &hold_process);
    if (hold_service == nullptr || hold_process == nullptr) {
        release_controlling_terminal(*shell_process);
        destroy_process(shell_process);
        set_service_state(init_service, xinim::kernel::recovery::ServiceState::CRASHED);
        return false;
    }

#ifdef XINIM_ARCH_I686
    initialize_i686_extensions();
#else
    initialize_legacy_pic();
    initialize_pit(kTimerHz);
#endif
    initialize_realtime_clock();
    ext2_reader::set_timestamp_provider(current_epoch_seconds);

    console::write_string("i486 user backing live bytes=");
    console::write_dec32(user_backing::live_bytes());
    console::write_string(" reserved bytes=");
    console::write_dec32(user_backing::reserved_bytes());
    console::newline();
    console::write_string("Launching supervised Ring 3 services under timer scheduler");
    console::newline();
    dispatch_next_runnable("failed to select initial supervised i486 service");
}

} // namespace xinim::i486::ring3
