#include "ring3.hpp"

#include "bootfs.hpp"
#include "console.hpp"
#include "elf32_loader.hpp"
#include "ext2_reader.hpp"
#include "../scheduler_policy.hpp"
#include "../recovery/recovery_dag.hpp"
#include "shell.hpp"
#include "socket_i486.hpp"
#include "../recovery/service_node.hpp"
#include "xinim/sys/syscalls.h"

namespace xinim::i486::ring3 {
namespace {

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

constexpr uint16_t kKernelCodeSelector = 0x08U;
constexpr uint16_t kKernelDataSelector = 0x10U;
constexpr uint16_t kUserCodeSelector = 0x1BU;
constexpr uint16_t kUserDataSelector = 0x23U;
constexpr uint16_t kTssSelector = 0x28U;
constexpr uint8_t kTimerVector = 32U;
constexpr uint8_t kSyscallVector = 0x80U;
constexpr uint32_t kKernelStackSize = 8192U;
constexpr size_t kMaxProcesses = 8U;
constexpr uint32_t kUserEflags = 0x202U;
constexpr uint32_t kPageSize = 4096U;
constexpr uint32_t kMaxExecArgs = 16U;
constexpr uint32_t kMaxExecEnvs = 16U;
constexpr uint32_t kMaxExecStringBytes = 512U;
constexpr uint32_t kWaitNoHang = 1U;
constexpr uint32_t kWaitUntraced = 2U;
constexpr int32_t kWaitPidAny = -1;
constexpr uint32_t kHeapGuardBytes = 64U * 1024U;
constexpr uint32_t kAuxvTagNull = 0U;
constexpr uint32_t kAuxvTagPageSize = 6U;
constexpr uint32_t kMaxUserMappings = 16U;
constexpr uint32_t kMapPrivate = 0x02U;
constexpr uint32_t kMapFixed = 0x10U;
constexpr uint32_t kMapAnonymous = 0x20U;
constexpr uint32_t kErrnoNoSys = static_cast<uint32_t>(-38);
constexpr uint32_t kErrnoIntr = static_cast<uint32_t>(-4);
constexpr uint32_t kErrnoNoEnt = static_cast<uint32_t>(-2);
constexpr uint32_t kErrnoAcces = static_cast<uint32_t>(-13);
constexpr uint32_t kErrnoBadF = static_cast<uint32_t>(-9);
constexpr uint32_t kErrnoChild = static_cast<uint32_t>(-10);
constexpr uint32_t kErrnoNoMem = static_cast<uint32_t>(-12);
constexpr uint32_t kErrnoFault = static_cast<uint32_t>(-14);
constexpr uint32_t kErrnoInvalid = static_cast<uint32_t>(-22);
constexpr uint32_t kErrnoNoTTY = static_cast<uint32_t>(-25);
constexpr uint16_t kPic1CommandPort = 0x20U;
constexpr uint16_t kPic1DataPort = 0x21U;
constexpr uint16_t kPic2CommandPort = 0xA0U;
constexpr uint16_t kPic2DataPort = 0xA1U;
constexpr uint16_t kPitChannel0Port = 0x40U;
constexpr uint16_t kPitModePort = 0x43U;
constexpr uint8_t kPicInitialize = 0x11U;
constexpr uint8_t kPic8086Mode = 0x01U;
constexpr uint8_t kPicEoi = 0x20U;
constexpr uint32_t kTimerHz = 100U;
constexpr uint32_t kPitInputHz = 1193182U;
constexpr size_t kMaxSupervisedServices = 8U;
constexpr uint8_t kInitServiceMaxRestarts = 16U;
constexpr uint8_t kHoldServiceMaxRestarts = 4U;
constexpr uint32_t kInitServicePriority = xinim::kernel::sched_policy::PRIO_USER_NORM;
constexpr uint32_t kSupportServicePriority = xinim::kernel::sched_policy::PRIO_USER_LOW;

struct TimeVal32 {
    uint32_t seconds;
    uint32_t microseconds;
};

struct TimeSpec32 {
    uint32_t seconds;
    uint32_t nanoseconds;
};

struct RLimit32 {
    uint32_t current;
    uint32_t maximum;
};

struct RUsage32 {
    TimeVal32 user_time;
    TimeVal32 system_time;
    int32_t max_rss;
    int32_t integral_shared_rss;
    int32_t integral_unshared_data;
    int32_t integral_unshared_stack;
    int32_t minor_faults;
    int32_t major_faults;
    int32_t swaps;
    int32_t block_inputs;
    int32_t block_outputs;
    int32_t messages_sent;
    int32_t messages_received;
    int32_t signals_received;
    int32_t voluntary_context_switches;
    int32_t involuntary_context_switches;
};

struct UserMapping {
    bool in_use;
    uint32_t address;
    uint32_t size;
};

struct [[gnu::packed]] GdtEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
};

struct [[gnu::packed]] TssEntry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt_selector;
    uint16_t trap;
    uint16_t iomap_base;
};

struct [[gnu::packed]] GdtDescriptor {
    uint16_t size;
    uint32_t offset;
};

struct [[gnu::packed]] IdtEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
};

struct [[gnu::packed]] IdtDescriptor {
    uint16_t size;
    uint32_t offset;
};

struct RegisterFrame {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
};

struct UserContext {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
};

// -- Signal definitions for i486 (32-bit) ---------------------------------

constexpr uint32_t kSigHup = 1U;
constexpr uint32_t kSigInt = 2U;
constexpr uint32_t kSigQuit = 3U;
constexpr uint32_t kSigIll = 4U;
constexpr uint32_t kSigAbrt = 6U;
constexpr uint32_t kSigKill = 9U;
constexpr uint32_t kSigSegv = 11U;
constexpr uint32_t kSigPipe = 13U;
constexpr uint32_t kSigAlrm = 14U;
constexpr uint32_t kSigTerm = 15U;
constexpr uint32_t kSigChld = 17U;
constexpr uint32_t kSigCont = 18U;
constexpr uint32_t kSigStop = 19U;
constexpr uint32_t kSigTstp = 20U;
constexpr uint32_t kSigTtin = 21U;
constexpr uint32_t kMaxSignals = 32U;

constexpr uint32_t kSigDfl = 0U;
constexpr uint32_t kSigIgn = 1U;

// SA_* flags (subset matching Linux i386 ABI)
constexpr uint32_t kSaRestart = 0x10000000U;
constexpr uint32_t kSaNodefer = 0x40000000U;
constexpr uint32_t kSaResethand = 0x80000000U;
constexpr uint32_t kSaNocldwait = 0x00000002U;


struct SignalHandler32 {
    uint32_t handler;   // SIG_DFL(0), SIG_IGN(1), or user function address
    uint32_t flags;     // SA_* flags
    uint32_t mask;      // Signals blocked during handler execution
    uint32_t restorer;  // Signal restorer (sa_restorer)
};

struct SignalState32 {
    SignalHandler32 handlers[kMaxSignals]; // indices 0-31, only 1-31 used
    uint32_t pending;   // Pending signals bitmask
    uint32_t blocked;   // Blocked signals bitmask
    bool in_handler;    // Currently executing signal handler
    uint32_t saved_mask; // Saved signal mask during handler
};

// Signal frame pushed onto user stack before entering handler.
// sigreturn reads this back to restore original context.
struct [[gnu::packed]] SignalFrame32 {
    uint32_t sigreturn_trampoline[2]; // Code: movl $SYS_rt_sigreturn,%eax; int $0x80
    uint32_t signum;
    UserContext saved_context;
    uint32_t saved_mask;
};

enum class ProcessState : uint8_t {
    Empty = 0,
    Runnable = 1,
    Waiting = 2,
    Exited = 3,
    Stopped = 4,
};

struct Process {
    bool in_use;
    uint32_t pid;
    uint32_t ppid;
    ProcessState state;
    uint32_t exit_status;
    uint32_t file_creation_mask;
    uint32_t minimum_break;
    uint32_t current_break;
    uint32_t saved_kernel_esp;
    uint32_t segment_base;
    uint32_t priority;
    uint32_t base_priority;
    uint32_t quantum_ticks;
    uint16_t scheduler_domain;
    bool waiting_for_console_input;
    uint64_t wake_tick;
    uint32_t ticks_remaining;
    uint32_t pgid;
    uint64_t alarm_tick; // Scheduler tick when SIGALRM should fire (0 = disabled)
    int ctty_slot; // Global OpenFile slot for controlling terminal (-1 = none)
    char cwd[256]; // Current working directory (absolute path)
    SignalState32 signals;
    int fd_map[32]; // Per-process fd table: maps local fd -> global OpenFile slot (-1 = unused)
    UserMapping mappings[kMaxUserMappings];
    UserContext context;
    alignas(16) uint8_t kernel_stack[kKernelStackSize];
    alignas(16) uint8_t address_space[elf32::kUserAddressSpaceSize];
};

template <uint32_t Count>
struct ExecVector {
    uint32_t count;
    const char* values[Count];
    char storage[kMaxExecStringBytes];
    uint32_t used_bytes;
};

enum class ServiceLaunchMode : uint8_t {
    BootOnly = 0,
    OnDemand = 1,
};

struct SupervisedService {
    bool in_use;
    uint32_t service_id;
    char name[xinim::kernel::recovery::MAX_SERVICE_NAME];
    const bootfs::FileRecord* file;
    const char* path;
    const char* env;
    uint32_t pid;
    xinim::kernel::recovery::RestartPolicy restart_policy;
    xinim::kernel::recovery::ServiceState state;
    uint8_t restart_count;
    uint8_t max_restarts;
    bool respawn_on_clean_exit;
    bool critical;
    uint32_t owner_pid;
    int dag_index;
    ServiceLaunchMode launch_mode;
    uint32_t priority;
    uint32_t base_priority;
    uint32_t quantum_ticks;
    uint16_t scheduler_domain;
    bool run_announced;
};

alignas(16) GdtEntry g_gdt[7]{};
alignas(16) TssEntry g_tss{};
alignas(16) IdtEntry g_idt[256]{};
alignas(16) uint8_t g_bootstrap_kernel_stack[kKernelStackSize]{};
alignas(16) Process g_processes[kMaxProcesses]{};
SupervisedService g_supervised_services[kMaxSupervisedServices]{};
xinim::kernel::recovery::RecoveryDag g_service_recovery_dag{};
const xinim::boot::BootInfo* g_boot_info = nullptr;
Process* g_current_process = nullptr;
uint32_t g_next_pid = 1U;
uint32_t g_next_service_id = 1U;
// Syscall trace disabled -- was polluting VGA via console::write_string
uint64_t g_scheduler_ticks = 0U;

inline void outb(uint16_t port, uint8_t value) noexcept {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

extern "C" void i486_load_gdt(uint32_t descriptor) noexcept;
extern "C" void i486_load_idt(uint32_t descriptor) noexcept;
extern "C" void i486_load_tss(uint16_t selector) noexcept;
extern "C" void i486_resume_user_context(const UserContext* context) noexcept;
extern "C" void i486_switch_to_user_context(const UserContext* context,
                                             uint32_t* saved_kernel_esp) noexcept;
extern "C" void i486_resume_saved_kernel_stack(uint32_t saved_kernel_esp) noexcept;
extern "C" void i486_syscall_entry() noexcept;
extern "C" void i486_timer_irq_entry() noexcept;
extern "C" void i486_fault_ud_entry() noexcept;
extern "C" void i486_fault_gp_entry() noexcept;
extern "C" void i486_fault_pf_entry() noexcept;

[[nodiscard]] bool string_equals(const char* lhs, const char* rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

void zero_region(uint8_t* base, uint32_t size) noexcept {
    for (uint32_t index = 0U; index < size; ++index) {
        base[index] = 0U;
    }
}

void copy_region(uint8_t* out, const uint8_t* in, uint32_t size) noexcept {
    for (uint32_t index = 0U; index < size; ++index) {
        out[index] = in[index];
    }
}

[[nodiscard]] uint32_t string_length(const char* text) noexcept {
    if (text == nullptr) {
        return 0U;
    }
    uint32_t length = 0U;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

void copy_c_string(char* destination, uint32_t capacity, const char* source) noexcept {
    if (destination == nullptr || capacity == 0U) {
        return;
    }
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }
    uint32_t index = 0U;
    while (index + 1U < capacity && source[index] != '\0') {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

// Translate a process-local fd through fd_map to a global OpenFile slot index.
// Returns -1 if the fd is invalid or unmapped.
[[nodiscard]] int resolve_fd(const Process* process, int fd) noexcept {
    if (process == nullptr || fd < 0 || fd >= 32) {
        return -1;
    }
    return process->fd_map[fd];
}

// Allocate the lowest available fd_map slot for a process, mapping it to the
// given global slot. Returns the new local fd, or -1 if no slot is free.
[[nodiscard]] int allocate_fd_map_entry(Process* process, int global_slot) noexcept {
    if (process == nullptr || global_slot < 0) {
        return -1;
    }
    for (int i = 0; i < 32; ++i) {
        if (process->fd_map[i] == -1) {
            process->fd_map[i] = global_slot;
            return i;
        }
    }
    return -1;
}

[[nodiscard]] uint32_t align_down(uint32_t value, uint32_t alignment) noexcept {
    return value & ~(alignment - 1U);
}

[[nodiscard]] uint32_t align_up(uint32_t value, uint32_t alignment) noexcept {
    return (value + alignment - 1U) & ~(alignment - 1U);
}

[[nodiscard]] bool copy_user_string(Process* process, uint32_t user_address,
                                    char* buffer, uint32_t capacity) noexcept;

// Resolve a relative path against the process's cwd.
// If path starts with '/', it's already absolute. Otherwise prepend cwd.
bool resolve_path(const Process* process,
                  const char* input,
                  char* output,
                  uint32_t capacity) noexcept {
    if (process == nullptr || input == nullptr || output == nullptr || capacity == 0U) {
        return false;
    }
    if (input[0] == '/' || input[0] == '\0') {
        copy_c_string(output, capacity, input);
        return true;
    }
    // Prepend cwd + "/" + input
    const uint32_t cwd_len = string_length(process->cwd);
    const uint32_t input_len = string_length(input);
    const bool needs_slash = (cwd_len > 0U && process->cwd[cwd_len - 1U] != '/');
    const uint32_t total = cwd_len + (needs_slash ? 1U : 0U) + input_len + 1U;
    if (total > capacity) {
        return false;
    }
    uint32_t pos = 0U;
    for (uint32_t i = 0U; i < cwd_len; ++i) output[pos++] = process->cwd[i];
    if (needs_slash) output[pos++] = '/';
    for (uint32_t i = 0U; i < input_len; ++i) output[pos++] = input[i];
    output[pos] = '\0';
    return true;
}

// Copy user path string and resolve against cwd
bool copy_and_resolve_user_path(Process* process,
                                uint32_t user_address,
                                char* buffer,
                                uint32_t capacity) noexcept {
    char raw[256]{};
    if (!copy_user_string(process, user_address, raw, sizeof(raw))) {
        return false;
    }
    return resolve_path(process, raw, buffer, capacity);
}

void activate_process(Process* process) noexcept;
void init_signal_state(Process* process) noexcept;
void send_signal_to_process(Process* target, uint32_t signum) noexcept;
bool deliver_one_signal(Process* process) noexcept;
[[nodiscard]] Process* find_process(uint32_t pid) noexcept;
[[noreturn]] void resume_waiting_parent(Process* parent) noexcept;
[[nodiscard]] SupervisedService* find_supervised_service_by_process(
    const Process* process) noexcept;

void set_gdt_entry(int index,
                   uint32_t base,
                   uint32_t limit,
                   uint8_t access,
                   uint8_t granularity) noexcept {
    g_gdt[index].limit_low = static_cast<uint16_t>(limit & 0xFFFFU);
    g_gdt[index].base_low = static_cast<uint16_t>(base & 0xFFFFU);
    g_gdt[index].base_middle = static_cast<uint8_t>((base >> 16U) & 0xFFU);
    g_gdt[index].access = access;
    g_gdt[index].granularity =
        static_cast<uint8_t>(((limit >> 16U) & 0x0FU) | (granularity & 0xF0U));
    g_gdt[index].base_high = static_cast<uint8_t>((base >> 24U) & 0xFFU);
}

void set_tss_descriptor(int index, uint32_t base, uint32_t limit) noexcept {
    set_gdt_entry(index, base, limit, 0x89U, 0x00U);
    g_gdt[index + 1] = {};
}

void set_idt_gate(uint8_t vector, void (*handler)() noexcept) noexcept {
    const uint32_t address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handler));
    g_idt[vector].offset_low = static_cast<uint16_t>(address & 0xFFFFU);
    g_idt[vector].selector = kKernelCodeSelector;
    g_idt[vector].zero = 0U;
    g_idt[vector].type_attr = 0xEEU;
    g_idt[vector].offset_high = static_cast<uint16_t>((address >> 16U) & 0xFFFFU);
}

void set_kernel_fault_gate(uint8_t vector, void (*handler)() noexcept) noexcept {
    const uint32_t address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handler));
    g_idt[vector].offset_low = static_cast<uint16_t>(address & 0xFFFFU);
    g_idt[vector].selector = kKernelCodeSelector;
    g_idt[vector].zero = 0U;
    g_idt[vector].type_attr = 0x8EU;
    g_idt[vector].offset_high = static_cast<uint16_t>((address >> 16U) & 0xFFFFU);
}

void set_user_segment_base(uint32_t base) noexcept {
    // Page granularity: limit covers kUserVirtualBase + kUserAddressSpaceSize.
    // Virtual addresses start at kUserVirtualBase (0x400000) and the segment
    // base is address_space - kUserVirtualBase, so offset 0x400000 maps to
    // address_space[0]. The limit must allow access up to offset
    // kUserVirtualBase + kUserAddressSpaceSize - 1.
    const uint32_t limit_pages =
        ((elf32::kUserVirtualBase + elf32::kUserAddressSpaceSize) / kPageSize) - 1U;
    // 0xC0 = G=1 (page granularity) | D=1 (32-bit segment)
    set_gdt_entry(3, base, limit_pages, 0xFAU, 0xC0U);
    set_gdt_entry(4, base, limit_pages, 0xF2U, 0xC0U);
}

void initialize_protection() noexcept {
    set_gdt_entry(0, 0U, 0U, 0U, 0U);
    set_gdt_entry(1, 0U, 0x000FFFFFU, 0x9AU, 0xCFU);
    set_gdt_entry(2, 0U, 0x000FFFFFU, 0x92U, 0xCFU);
    set_user_segment_base(0U);

    g_tss = {};
    g_tss.ss0 = kKernelDataSelector;
    g_tss.esp0 = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(g_bootstrap_kernel_stack + sizeof(g_bootstrap_kernel_stack)));
    g_tss.iomap_base = sizeof(TssEntry);
    set_tss_descriptor(5,
                       static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_tss)),
                       static_cast<uint32_t>(sizeof(TssEntry) - 1U));

    const GdtDescriptor gdt_descriptor{
        static_cast<uint16_t>(sizeof(g_gdt) - 1U),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_gdt)),
    };
    i486_load_gdt(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&gdt_descriptor)));
    i486_load_tss(kTssSelector);

    for (auto& entry : g_idt) {
        entry = {};
    }
    set_kernel_fault_gate(kTimerVector, i486_timer_irq_entry);
    set_idt_gate(kSyscallVector, i486_syscall_entry);
    set_kernel_fault_gate(6U, i486_fault_ud_entry);
    set_kernel_fault_gate(13U, i486_fault_gp_entry);
    set_kernel_fault_gate(14U, i486_fault_pf_entry);
    const IdtDescriptor idt_descriptor{
        static_cast<uint16_t>(sizeof(g_idt) - 1U),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_idt)),
    };
    i486_load_idt(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&idt_descriptor)));
}

void initialize_legacy_pic() noexcept {
    outb(kPic1DataPort, 0xFFU);
    outb(kPic2DataPort, 0xFFU);

    outb(kPic1CommandPort, kPicInitialize);
    outb(kPic2CommandPort, kPicInitialize);
    outb(kPic1DataPort, kTimerVector);
    outb(kPic2DataPort, static_cast<uint8_t>(kTimerVector + 8U));
    outb(kPic1DataPort, 0x04U);
    outb(kPic2DataPort, 0x02U);
    outb(kPic1DataPort, kPic8086Mode);
    outb(kPic2DataPort, kPic8086Mode);

    outb(kPic1DataPort, 0xFEU);
    outb(kPic2DataPort, 0xFFU);
}

void initialize_pit(uint32_t frequency_hz) noexcept {
    const uint32_t divisor = frequency_hz == 0U ? 0U : (kPitInputHz / frequency_hz);
    outb(kPitModePort, 0x36U);
    outb(kPitChannel0Port, static_cast<uint8_t>(divisor & 0xFFU));
    outb(kPitChannel0Port, static_cast<uint8_t>((divisor >> 8U) & 0xFFU));
}

void send_timer_eoi() noexcept {
    outb(kPic1CommandPort, kPicEoi);
}

[[noreturn]] void resume_rescue_shell(const char* reason) noexcept {
    console::write_string(reason);
    console::newline();
    console::write_string("Falling back to rescue shell on COM2");
    console::newline();
    if (g_boot_info != nullptr) {
        xinim::i486::shell::run(*g_boot_info);
    }
    for (;;) {
        asm volatile("cli; hlt");
    }
}

[[nodiscard]] UserContext capture_user_context(RegisterFrame* frame) noexcept {
    UserContext context{};
    if (frame == nullptr) {
        return context;
    }
    const auto* tail = reinterpret_cast<const uint32_t*>(
        reinterpret_cast<const uint8_t*>(frame) + sizeof(RegisterFrame));
    context.eax = frame->eax;
    context.ebx = frame->ebx;
    context.ecx = frame->ecx;
    context.edx = frame->edx;
    context.esi = frame->esi;
    context.edi = frame->edi;
    context.ebp = frame->ebp;
    context.ds = frame->ds;
    context.es = frame->es;
    context.fs = frame->fs;
    context.gs = frame->gs;
    context.eip = tail[0];
    context.cs = tail[1];
    context.eflags = tail[2];
    context.esp = tail[3];
    context.ss = tail[4];
    return context;
}

[[nodiscard]] uint32_t effective_quantum(const Process* process) noexcept {
    if (process == nullptr) {
        return 0U;
    }
    if (process->quantum_ticks != 0U) {
        return process->quantum_ticks;
    }
    return xinim::kernel::sched_policy::quantum_for_priority(process->priority);
}

void apply_scheduler_profile(Process* process,
                             uint32_t priority,
                             uint32_t base_priority,
                             uint32_t quantum_ticks,
                             uint16_t scheduler_domain) noexcept {
    if (process == nullptr) {
        return;
    }
    process->priority = xinim::kernel::sched_policy::clamp_priority(priority);
    process->base_priority = xinim::kernel::sched_policy::clamp_priority(base_priority);
    process->quantum_ticks = quantum_ticks;
    process->scheduler_domain = scheduler_domain;
    process->ticks_remaining = effective_quantum(process);
}

void clear_saved_kernel_stack(Process* process) noexcept {
    if (process != nullptr) {
        process->saved_kernel_esp = 0U;
    }
}

[[nodiscard]] int process_slot_index(const Process* process) noexcept {
    if (process == nullptr) {
        return -1;
    }
    for (size_t index = 0; index < kMaxProcesses; ++index) {
        if (&g_processes[index] == process) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void wake_ready_waiters() noexcept {
    const bool have_console_input = console::tty_has_input();
    for (auto& process : g_processes) {
        if (!process.in_use || process.state != ProcessState::Waiting) {
            continue;
        }
        // Wake blocked readers when console input is available OR
        // periodically (every 50 ticks = 500ms) so pipe readers can
        // detect EOF when all writers close.
        if (process.waiting_for_console_input) {
            if (have_console_input || (g_scheduler_ticks % 50U) == 0U) {
                process.waiting_for_console_input = false;
                process.state = ProcessState::Runnable;
            }
        }
        if (process.wake_tick != 0U && process.wake_tick <= g_scheduler_ticks) {
            process.wake_tick = 0U;
            process.state = ProcessState::Runnable;
        }
    }
}

[[nodiscard]] Process* select_next_runnable(Process* preferred_current) noexcept {
    Process* candidate = nullptr;
    uint32_t best_priority = static_cast<uint32_t>(xinim::kernel::sched_policy::NUM_PRIORITIES);
    if (preferred_current != nullptr && preferred_current->in_use &&
        preferred_current->state == ProcessState::Runnable) {
        candidate = preferred_current;
        best_priority = preferred_current->priority;
    }
    const int preferred_index = process_slot_index(preferred_current);

    for (size_t offset = 1; offset <= kMaxProcesses; ++offset) {
        const size_t index = static_cast<size_t>(
            (preferred_index + static_cast<int>(offset) + static_cast<int>(kMaxProcesses)) %
            static_cast<int>(kMaxProcesses));
        Process& process = g_processes[index];
        if (!process.in_use || process.state != ProcessState::Runnable) {
            continue;
        }
        if (&process == preferred_current) {
            continue;
        }
        if (process.priority < best_priority) {
            candidate = &process;
            best_priority = process.priority;
        } else if (process.priority == best_priority && candidate == nullptr) {
            candidate = &process;
        }
    }

    return candidate;
}

[[noreturn]] void dispatch_process(Process* process) noexcept {
    if (process == nullptr) {
        resume_rescue_shell("no runnable i486 process");
    }
    SupervisedService* service = find_supervised_service_by_process(process);
    if (service != nullptr && !service->run_announced) {
        service->run_announced = true;
    }
    activate_process(process);
    if (process->saved_kernel_esp != 0U) {
        const uint32_t saved_kernel_esp = process->saved_kernel_esp;
        process->saved_kernel_esp = 0U;
        i486_resume_saved_kernel_stack(saved_kernel_esp);
    }
    // Deliver pending signals before returning to userspace
    if (process->state == ProcessState::Runnable) {
        deliver_one_signal(process);
        // If signal delivery killed the process, find next runnable
        if (process->state == ProcessState::Exited) {
            Process* parent = find_process(process->ppid);
            if (parent != nullptr && parent->state == ProcessState::Waiting) {
                resume_waiting_parent(parent);
            }
            wake_ready_waiters();
            Process* next = select_next_runnable(nullptr);
            if (next != nullptr) {
                dispatch_process(next);
            }
            resume_rescue_shell("signal killed last runnable process");
        }
    }
    if (process->ticks_remaining == 0U) {
        process->ticks_remaining = effective_quantum(process);
    }
    i486_resume_user_context(&process->context);
    for (;;) {
        asm volatile("cli; hlt");
    }
}

extern "C" [[noreturn]] void i486_handle_timer_irq(RegisterFrame* frame) noexcept {
    Process* current = g_current_process;
    if (current != nullptr && current->in_use) {
        current->context = capture_user_context(frame);
    }

    ++g_scheduler_ticks;

    // Poll keyboard and serial into the TTY rx buffer so blocked readers can wake
    console::tty_poll_input();

    // Check for Ctrl+C / Ctrl+Z from keyboard
    const uint32_t tty_signal = console::consume_pending_tty_signal();
    if (tty_signal != 0U && current != nullptr && current->in_use) {
        send_signal_to_process(current, tty_signal);
    }

    // Check alarm timers for all processes
    for (auto& proc : g_processes) {
        if (proc.in_use && proc.alarm_tick != 0U && proc.alarm_tick <= g_scheduler_ticks) {
            proc.alarm_tick = 0U;
            send_signal_to_process(&proc, kSigAlrm);
        }
    }

    wake_ready_waiters();

    Process* preferred = current;
    if (current != nullptr && current->in_use && current->state == ProcessState::Runnable) {
        if (current->ticks_remaining > 0U) {
            --current->ticks_remaining;
        }
        if (current->ticks_remaining == 0U) {
            current->priority =
                xinim::kernel::sched_policy::demote_priority(current->priority);
            current->ticks_remaining = effective_quantum(current);
            preferred = nullptr;
        }
    } else {
        preferred = nullptr;
    }

    Process* next = select_next_runnable(preferred);
    send_timer_eoi();
    if (next == nullptr) {
        resume_rescue_shell("timer interrupt found no runnable i486 process");
    }
    dispatch_process(next);
}

void initialize_context(Process* process,
                        const elf32::UserImage& image,
                        uint32_t eax_value) noexcept {
    zero_region(reinterpret_cast<uint8_t*>(&process->context),
                static_cast<uint32_t>(sizeof(process->context)));
    process->context.eax = eax_value;
    process->context.ds = kUserDataSelector;
    process->context.es = kUserDataSelector;
    process->context.fs = kUserDataSelector;
    process->context.gs = kUserDataSelector;
    process->context.eip = image.entry_point;
    process->context.cs = kUserCodeSelector;
    process->context.eflags = kUserEflags;
    process->context.esp = image.stack_top;
    process->context.ss = kUserDataSelector;
}

[[nodiscard]] uint32_t compute_segment_base(const Process& process) noexcept {
    return static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(process.address_space) - elf32::kUserVirtualBase);
}

void activate_process(Process* process) noexcept {
    if (process == nullptr) {
        return;
    }
    g_current_process = process;
    set_user_segment_base(process->segment_base);
    g_tss.esp0 = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(process->kernel_stack + sizeof(process->kernel_stack)));
}

[[nodiscard]] Process* allocate_process(uint32_t parent_pid) noexcept {
    for (auto& process : g_processes) {
        if (!process.in_use) {
            zero_region(reinterpret_cast<uint8_t*>(&process),
                        static_cast<uint32_t>(sizeof(process)));
            process.in_use = true;
            process.pid = g_next_pid++;
            process.ppid = parent_pid;
            process.state = ProcessState::Runnable;
            process.file_creation_mask = 0022U;
            process.pgid = process.pid;
            process.cwd[0] = '/';
            process.cwd[1] = '\0';
            process.segment_base = compute_segment_base(process);
            process.ctty_slot = -1; // No controlling terminal initially
            // Per-process fd table: 0/1/2 = console, rest unused
            for (int fd_index = 0; fd_index < 32; ++fd_index) {
                process.fd_map[fd_index] = -1;
            }
            process.fd_map[0] = 0; // stdin  -> global slot 0
            process.fd_map[1] = 1; // stdout -> global slot 1
            process.fd_map[2] = 2; // stderr -> global slot 2
            init_signal_state(&process);
            apply_scheduler_profile(
                &process,
                kInitServicePriority,
                kInitServicePriority,
                0U,
                1U);
            return &process;
        }
    }
    return nullptr;
}

void destroy_process(Process* process) noexcept {
    if (process == nullptr) {
        return;
    }
    process->in_use = false;
    process->pid = 0U;
    process->ppid = 0U;
    process->state = ProcessState::Empty;
    process->exit_status = 0U;
    process->saved_kernel_esp = 0U;
}

[[nodiscard]] Process* find_process(uint32_t pid) noexcept {
    for (auto& process : g_processes) {
        if (process.in_use && process.pid == pid) {
            return &process;
        }
    }
    return nullptr;
}

[[nodiscard]] Process* find_child(Process* parent, int32_t requested_pid) noexcept {
    if (parent == nullptr) {
        return nullptr;
    }
    for (auto& process : g_processes) {
        if (!process.in_use || process.ppid != parent->pid) {
            continue;
        }
        if (requested_pid == -1 || static_cast<int32_t>(process.pid) == requested_pid) {
            return &process;
        }
    }
    return nullptr;
}

[[nodiscard]] bool has_child(Process* parent) noexcept {
    if (parent == nullptr) {
        return false;
    }
    for (auto& process : g_processes) {
        if (process.in_use && process.ppid == parent->pid) {
            return true;
        }
    }
    return false;
}

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

[[nodiscard]] bool translate_user_region(Process* process,
                                         uint32_t user_address,
                                         uint32_t size,
                                         uint8_t** out) noexcept {
    if (process == nullptr || out == nullptr) {
        return false;
    }
    if (user_address < elf32::kUserVirtualBase) {
        return false;
    }
    const uint32_t offset = user_address - elf32::kUserVirtualBase;
    if (offset > elf32::kUserAddressSpaceSize) {
        return false;
    }
    if (size > elf32::kUserAddressSpaceSize - offset) {
        return false;
    }
    *out = process->address_space + offset;
    return true;
}

[[nodiscard]] bool read_user_u32(Process* process,
                                 uint32_t user_address,
                                 uint32_t* out) noexcept {
    if (out == nullptr) {
        return false;
    }
    uint8_t* raw = nullptr;
    if (!translate_user_region(process, user_address, sizeof(uint32_t), &raw)) {
        return false;
    }
    *out = *reinterpret_cast<uint32_t*>(raw);
    return true;
}

[[nodiscard]] bool copy_user_string(Process* process,
                                    uint32_t user_address,
                                    char* buffer,
                                    uint32_t capacity) noexcept {
    if (buffer == nullptr || capacity == 0U) {
        return false;
    }
    uint8_t* start = nullptr;
    if (!translate_user_region(process, user_address, 1U, &start)) {
        return false;
    }
    const uint32_t offset = user_address - elf32::kUserVirtualBase;
    const uint32_t remaining = elf32::kUserAddressSpaceSize - offset;
    for (uint32_t index = 0U; index < remaining && index + 1U < capacity; ++index) {
        buffer[index] = static_cast<char>(start[index]);
        if (buffer[index] == '\0') {
            return true;
        }
    }
    buffer[capacity - 1U] = '\0';
    return false;
}

template <uint32_t Count>
[[nodiscard]] bool append_exec_string(ExecVector<Count>* vector,
                                      const char* text) noexcept {
    if (vector == nullptr || text == nullptr || vector->count >= Count) {
        return false;
    }
    const uint32_t length = string_length(text) + 1U;
    if (length > kMaxExecStringBytes - vector->used_bytes) {
        return false;
    }
    char* destination = vector->storage + vector->used_bytes;
    copy_region(reinterpret_cast<uint8_t*>(destination),
                reinterpret_cast<const uint8_t*>(text),
                length);
    vector->values[vector->count] = destination;
    ++vector->count;
    vector->used_bytes += length;
    return true;
}

template <uint32_t Count>
[[nodiscard]] bool copy_exec_vector(Process* process,
                                    uint32_t user_vector_address,
                                    ExecVector<Count>* out,
                                    const char* fallback0 = nullptr) noexcept {
    if (out == nullptr) {
        return false;
    }
    zero_region(reinterpret_cast<uint8_t*>(out), static_cast<uint32_t>(sizeof(*out)));

    if (user_vector_address == 0U) {
        if (fallback0 != nullptr) {
            return append_exec_string(out, fallback0);
        }
        return true;
    }

    for (uint32_t index = 0U; index < Count; ++index) {
        uint32_t element_address = 0U;
        if (!read_user_u32(process,
                           user_vector_address + (index * sizeof(uint32_t)),
                           &element_address)) {
            return false;
        }
        if (element_address == 0U) {
            return out->count != 0U || fallback0 == nullptr ||
                   append_exec_string(out, fallback0);
        }

        char buffer[128]{};
        if (!copy_user_string(process, element_address, buffer, sizeof(buffer)) ||
            !append_exec_string(out, buffer)) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool write_user_bytes(Process* process,
                                    uint32_t user_address,
                                    const void* source,
                                    uint32_t size) noexcept {
    uint8_t* destination = nullptr;
    if (!translate_user_region(process, user_address, size, &destination)) {
        return false;
    }
    if (size == 0U) {
        return true;
    }
    copy_region(destination, reinterpret_cast<const uint8_t*>(source), size);
    return true;
}

[[nodiscard]] bool write_user_u32(Process* process,
                                  uint32_t user_address,
                                  uint32_t value) noexcept {
    uint8_t* raw = nullptr;
    if (!translate_user_region(process, user_address, sizeof(uint32_t), &raw)) {
        return false;
    }
    auto* out = reinterpret_cast<uint32_t*>(raw);
    *out = value;
    return true;
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

template <uint32_t ArgCount, uint32_t EnvCount>
[[nodiscard]] bool build_initial_user_stack(Process* process,
                                            const ExecVector<ArgCount>& argv,
                                            const ExecVector<EnvCount>& envp,
                                            uint32_t* out_stack_pointer) noexcept {
    if (process == nullptr || out_stack_pointer == nullptr || argv.count == 0U) {
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
        if (!write_user_bytes(process, stack_pointer, text, length)) {
            return false;
        }
        envp_addresses[index - 1U] = stack_pointer;
    }

    for (uint32_t index = argv.count; index > 0U; --index) {
        const char* text = argv.values[index - 1U];
        const uint32_t length = string_length(text) + 1U;
        stack_pointer -= length;
        if (!write_user_bytes(process, stack_pointer, text, length)) {
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
    if (!write_user_bytes(process,
                          stack_pointer,
                          auxv,
                          static_cast<uint32_t>(sizeof(auxv)))) {
        return false;
    }

    stack_pointer -= static_cast<uint32_t>((envp.count + 1U) * sizeof(uint32_t));
    for (uint32_t index = 0U; index < envp.count; ++index) {
        if (!write_user_u32(process,
                            stack_pointer + (index * sizeof(uint32_t)),
                            envp_addresses[index])) {
            return false;
        }
    }
    if (!write_user_u32(process,
                        stack_pointer + (envp.count * sizeof(uint32_t)),
                        0U)) {
        return false;
    }

    stack_pointer -= static_cast<uint32_t>((argv.count + 1U) * sizeof(uint32_t));
    for (uint32_t index = 0U; index < argv.count; ++index) {
        if (!write_user_u32(process,
                            stack_pointer + (index * sizeof(uint32_t)),
                            argv_addresses[index])) {
            return false;
        }
    }
    if (!write_user_u32(process,
                        stack_pointer + (argv.count * sizeof(uint32_t)),
                        0U)) {
        return false;
    }

    stack_pointer -= sizeof(uint32_t);
    if (!write_user_u32(process, stack_pointer, argv.count)) {
        return false;
    }

    *out_stack_pointer = stack_pointer;
    return true;
}

[[nodiscard]] bool load_process_image(Process* process,
                                      const bootfs::FileRecord* file,
                                      const ExecVector<kMaxExecArgs>& argv,
                                      const ExecVector<kMaxExecEnvs>& envp) noexcept {
    if (process == nullptr || file == nullptr) {
        return false;
    }
    zero_region(process->address_space, elf32::kUserAddressSpaceSize);
    zero_region(reinterpret_cast<uint8_t*>(process->mappings),
                static_cast<uint32_t>(sizeof(process->mappings)));
    elf32::UserImage image{};
    if (!elf32::load_static_image(file->data,
                                  file->size,
                                  process->address_space,
                                  elf32::kUserAddressSpaceSize,
                                  &image)) {
        return false;
    }
    if (!build_initial_user_stack(process, argv, envp, &image.stack_top)) {
        return false;
    }
    initialize_context(process, image, 0U);
    process->minimum_break = image.brk_start;
    process->current_break = image.brk_start;
    return true;
}

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
           append_exec_string(envp, "ENV=/etc/mkshrc") &&
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

[[nodiscard]] bool prepare_supervised_service_process(SupervisedService* service,
                                                      Process* process) noexcept {
    if (service == nullptr || process == nullptr || service->file == nullptr) {
        return false;
    }

    ExecVector<kMaxExecArgs> argv{};
    ExecVector<kMaxExecEnvs> envp{};
    if (!build_service_vectors(service, &argv, &envp) ||
        !load_process_image(process, service->file, argv, envp)) {
        return false;
    }

    process->state = ProcessState::Runnable;
    process->exit_status = 0U;
    process->saved_kernel_esp = 0U;
    process->waiting_for_console_input = false;
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

[[noreturn]] void dispatch_next_runnable(const char* rescue_reason) noexcept {
    wake_ready_waiters();
    Process* next = select_next_runnable(g_current_process);
    if (next == nullptr) {
        resume_rescue_shell(rescue_reason);
    }
    dispatch_process(next);
}

// Returns true if woken by a pending signal (caller should return -EINTR)
bool block_current_process_until_rescheduled(Process* process,
                                             bool waiting_for_console_input,
                                             uint64_t wake_tick) noexcept {
    if (process == nullptr) {
        resume_rescue_shell("attempted to block missing i486 process");
    }
    process->state = ProcessState::Waiting;
    process->waiting_for_console_input = waiting_for_console_input;
    process->wake_tick = wake_tick;
    wake_ready_waiters();
    Process* next = select_next_runnable(g_current_process);
    if (next == nullptr) {
        resume_rescue_shell("no runnable i486 process while current blocked");
    }
    activate_process(next);
    i486_switch_to_user_context(&next->context, &process->saved_kernel_esp);
    clear_saved_kernel_stack(process);
    process->state = ProcessState::Runnable;
    process->waiting_for_console_input = false;
    process->wake_tick = 0U;
    // Check if we were woken by a signal
    const bool has_signal = (process->signals.pending & ~process->signals.blocked) != 0U;
    activate_process(process);
    return has_signal;
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
        hold_service->state = xinim::kernel::recovery::ServiceState::CRASHED;
        return nullptr;
    }
    if (out_process != nullptr) {
        *out_process = hold_process;
    }
    console::write_string("Prepared supervised support service hold-service");
    console::newline();
    return hold_service;
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

[[noreturn]] void resume_waiting_parent(Process* parent) noexcept {
    activate_process(parent);
    const uint32_t saved_kernel_esp = parent->saved_kernel_esp;
    clear_saved_kernel_stack(parent);
    i486_resume_saved_kernel_stack(saved_kernel_esp);
    for (;;) {
        asm volatile("cli; hlt");
    }
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

[[noreturn]] void exit_current_process(uint32_t exit_status,
                                       const char* fallback_reason,
                                       bool crashed = false) noexcept {
    Process* process = g_current_process;
    if (process == nullptr) {
        resume_rescue_shell(fallback_reason);
    }

    process->exit_status = exit_status;
    process->state = ProcessState::Exited;

    // Release all fd_map entries, decrement refcounts on global slots
    for (int fdi = 0; fdi < 32; ++fdi) {
        const int slot = process->fd_map[fdi];
        if (slot >= 0) {
            process->fd_map[fdi] = -1;
            bootfs::decrement_slot_refcount(slot);
        }
    }

    // SIGHUP: when session leader exits, send SIGHUP to process group
    if (process->ctty_slot >= 0) {
        for (auto& peer : g_processes) {
            if (peer.in_use && peer.pgid == process->pgid && &peer != process) {
                send_signal_to_process(&peer, kSigHup);
            }
        }
    }

    // Reparent orphaned children to PID 1 (init)
    for (auto& child : g_processes) {
        if (child.in_use && child.ppid == process->pid) {
            child.ppid = 1U;
        }
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
    }

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

    if (parent != nullptr) {
        dispatch_next_runnable("no runnable i486 process after child exit");
    }

    destroy_process(process);
    dispatch_next_runnable(fallback_reason);
}

[[nodiscard]] uint32_t wait_status_for_exit(uint32_t exit_status) noexcept {
    return (exit_status & 0xFFU) << 8U;
}

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
    if (reading_console) {
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
                if (block_current_process_until_rescheduled(process, true, 0U)) {
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
            if (block_current_process_until_rescheduled(process, true, 0U)) {
                return kErrnoIntr; // Interrupted by signal
            }
            continue;
        }
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

    for (;;) {
        const int result = bootfs::write(fd, buffer, count);
        if (result == bootfs::kWriteWouldBlock) {
            if (block_current_process_until_rescheduled(process, false, 0U)) {
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
    return static_cast<uint32_t>(new_local_fd);
}

[[nodiscard]] uint32_t sys_dup2(Process* process, RegisterFrame* frame) noexcept {
    const int old_user_fd = static_cast<int>(frame->ebx);
    const int new_user_fd = static_cast<int>(frame->ecx);
    if (new_user_fd < 0 || new_user_fd >= 32) {
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
    const int global_slot = bootfs::open(path, frame->ecx, frame->edx);
    if (global_slot < 0) {
        return static_cast<uint32_t>(global_slot);
    }
    const int local_fd = allocate_fd_map_entry(process, global_slot);
    if (local_fd < 0) {
        bootfs::close(global_slot);
        return kErrnoNoMem;
    }
    // Set controlling terminal when opening /dev/tty or /dev/console
    if (process->ctty_slot < 0 && bootfs::is_console_fd(global_slot)) {
        process->ctty_slot = global_slot;
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
    case 0: {
        // F_DUPFD: dup to lowest available fd >= arg
        if (!bootfs::is_open(fd)) {
            return kErrnoBadF;
        }
        const int new_local = allocate_fd_map_entry(process, fd);
        if (new_local < 0) {
            return kErrnoNoMem;
        }
        bootfs::increment_slot_refcount(fd);
        return static_cast<uint32_t>(new_local);
    }
    case 1: {
        const int result = bootfs::descriptor_flags(fd);
        return result >= 0 ? static_cast<uint32_t>(result) : kErrnoBadF;
    }
    case 2:
        return bootfs::set_descriptor_flags(fd, static_cast<int>(frame->edx)) == 0
            ? 0U
            : kErrnoBadF;
    case 3: {
        const int result = bootfs::status_flags(fd);
        return result >= 0 ? static_cast<uint32_t>(result) : kErrnoBadF;
    }
    case 4:
        return bootfs::set_status_flags(fd, static_cast<int>(frame->edx)) == 0
            ? 0U
            : kErrnoBadF;
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
        return result == 0 ? 0U : kErrnoNoTTY;
    }

    const int result = bootfs::control(
        fd,
        static_cast<int>(frame->ecx),
        0U);
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
    child->pgid = process->pgid;
    child->ctty_slot = process->ctty_slot;
    copy_c_string(child->cwd, static_cast<uint32_t>(sizeof(child->cwd)), process->cwd);
    // Copy per-process fd table; increment refcounts on shared global slots
    for (int fdi = 0; fdi < 32; ++fdi) {
        child->fd_map[fdi] = process->fd_map[fdi];
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
            path,
            const_cast<uint8_t*>(image),
            image_size,
            image_size,
            true,
            true,
            false,
        };
        file = &ext2_file;
    } else {
        file = bootfs::find(path);
    }

    if (file == nullptr || !file->executable || !load_process_image(process, file, argv, envp)) {
        process->context = capture_user_context(frame);
        process->context.eax = kErrnoNoEnt;
        activate_process(process);
        i486_resume_user_context(&process->context);
        __builtin_unreachable();
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

    // POSIX: execve closes FD_CLOEXEC file descriptors via per-process fd_map
    for (int fdi = 0; fdi < 32; ++fdi) {
        const int slot = process->fd_map[fdi];
        if (slot < 0) {
            continue;
        }
        const int flags = bootfs::descriptor_flags_for_slot(slot);
        if (flags >= 0 && (flags & 1) != 0) { // FD_CLOEXEC
            process->fd_map[fdi] = -1;
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
    if (child == nullptr) {
        if (no_hang) {
            return 0U;
        }

        Process* runnable_child = find_child(process, requested_pid);
        if (runnable_child == nullptr) {
            return kErrnoChild;
        }
        process->state = ProcessState::Waiting;
        activate_process(runnable_child);
        i486_switch_to_user_context(&runnable_child->context, &process->saved_kernel_esp);
        clear_saved_kernel_stack(process);
        process->state = ProcessState::Runnable;
        activate_process(process);

        child = find_waiting_child(process, requested_pid, wuntraced);
        if (child == nullptr) {
            return kErrnoChild;
        }
    }

    if (child->state != ProcessState::Exited) {
        process->state = ProcessState::Waiting;
        activate_process(child);
        i486_switch_to_user_context(&child->context, &process->saved_kernel_esp);
        clear_saved_kernel_stack(process);
        process->state = ProcessState::Runnable;
        activate_process(process);
        child = find_waiting_child(process, requested_pid, wuntraced);
        if (child == nullptr) {
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
    const int32_t fd = static_cast<int32_t>(frame->edi);
    const uint32_t page_offset = frame->ebp;

    if (length == 0U) {
        return kErrnoInvalid;
    }
    if ((flags & kMapFixed) != 0U || address != 0U) {
        return kErrnoInvalid;
    }
    if ((flags & (kMapPrivate | kMapAnonymous)) != (kMapPrivate | kMapAnonymous)) {
        return kErrnoInvalid;
    }
    if (fd != -1 || page_offset != 0U) {
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

    slot->in_use = true;
    slot->address = mapping_base;
    slot->size = length;
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

[[nodiscard]] uint32_t sys_mprotect_compat(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    (void)frame;
    return 0U;
}

// -- RTC and time support --------------------------------------------------

inline uint8_t inb_port(uint16_t port) noexcept {
    uint8_t value = 0U;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint8_t read_cmos(uint8_t reg) noexcept {
    outb(0x70U, reg);
    return inb_port(0x71U);
}

uint8_t bcd_to_bin(uint8_t bcd) noexcept {
    return static_cast<uint8_t>((bcd >> 4U) * 10U + (bcd & 0x0FU));
}

// Days from year 0 to start of each month (non-leap, cumulative)
uint32_t days_in_months(uint32_t month, bool leap) noexcept {
    static constexpr uint16_t kCum[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    if (month > 11U) month = 11U;
    uint32_t days = kCum[month];
    if (leap && month >= 2U) ++days;
    return days;
}

uint32_t read_rtc_epoch() noexcept {
    // Wait for RTC update to complete
    while ((read_cmos(0x0AU) & 0x80U) != 0U) {}
    uint8_t sec = bcd_to_bin(read_cmos(0x00U));
    uint8_t min = bcd_to_bin(read_cmos(0x02U));
    uint8_t hour = bcd_to_bin(read_cmos(0x04U));
    uint8_t day = bcd_to_bin(read_cmos(0x07U));
    uint8_t month = bcd_to_bin(read_cmos(0x08U));
    uint8_t year_bcd = bcd_to_bin(read_cmos(0x09U));
    uint32_t year = 2000U + static_cast<uint32_t>(year_bcd);

    // Convert to Unix epoch (seconds since 1970-01-01)
    uint32_t days = 0U;
    for (uint32_t y = 1970U; y < year; ++y) {
        bool leap = (y % 4U == 0U && (y % 100U != 0U || y % 400U == 0U));
        days += leap ? 366U : 365U;
    }
    bool cur_leap = (year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U));
    if (month > 0U) days += days_in_months(month - 1U, cur_leap);
    if (day > 0U) days += day - 1U;

    return days * 86400U + static_cast<uint32_t>(hour) * 3600U +
           static_cast<uint32_t>(min) * 60U + static_cast<uint32_t>(sec);
}

uint32_t g_boot_epoch_seconds = 0U;
uint64_t g_boot_epoch_ticks = 0U;

void initialize_realtime_clock() noexcept {
    g_boot_epoch_seconds = read_rtc_epoch();
    g_boot_epoch_ticks = g_scheduler_ticks;
}

uint32_t current_epoch_seconds() noexcept {
    const uint64_t elapsed_ticks = g_scheduler_ticks - g_boot_epoch_ticks;
    return g_boot_epoch_seconds + static_cast<uint32_t>(elapsed_ticks / kTimerHz);
}

uint32_t current_epoch_microseconds() noexcept {
    const uint64_t elapsed_ticks = g_scheduler_ticks - g_boot_epoch_ticks;
    const uint32_t sub_second_ticks = static_cast<uint32_t>(elapsed_ticks % kTimerHz);
    return sub_second_ticks * (1000000U / kTimerHz);
}

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
    // Create new session: new pgid = pid, detach from ctty
    process->pgid = process->pid;
    process->ctty_slot = -1;
    return process->pid;
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

[[nodiscard]] uint32_t sys_rt_sigprocmask_compat(Process* process, RegisterFrame* frame) noexcept {
    if (frame->edx != 0U) {
        const uint32_t words[32] = {};
        const uint32_t requested = frame->esi;
        const uint32_t available = static_cast<uint32_t>(sizeof(words));
        const uint32_t to_copy = requested < available ? requested : available;
        if (!write_user_bytes(process, frame->edx, words, to_copy)) {
            return kErrnoFault;
        }
    }
    return 0U;
}

// -- Phase 1.6 syscall: getdents ------------------------------------------

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

// -- Phase 2 syscalls: job control ----------------------------------------

[[nodiscard]] uint32_t sys_setpgid(Process* process, RegisterFrame* frame) noexcept {
    const uint32_t target_pid = frame->ebx;
    const uint32_t pgid = frame->ecx;
    const uint32_t effective_pid = (target_pid == 0U) ? process->pid : target_pid;
    const uint32_t effective_pgid = (pgid == 0U) ? effective_pid : pgid;

    Process* target = find_process(effective_pid);
    if (target == nullptr) {
        return kErrnoNoSys; // ESRCH
    }
    // Only allow setting pgid of self or child
    if (target->pid != process->pid && target->ppid != process->pid) {
        return kErrnoAcces;
    }
    target->pgid = effective_pgid;
    return 0U;
}

[[nodiscard]] uint32_t sys_getpgrp(Process* process, RegisterFrame* frame) noexcept {
    (void)frame;
    return process->pgid;
}

[[nodiscard]] uint32_t sys_getsid(Process* process, RegisterFrame* frame) noexcept {
    (void)frame;
    // In our single-session model, session ID equals PID of init
    return process->pgid;
}

// -- Phase 2 syscalls: uname ----------------------------------------------

struct UtsName32 {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

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

// -- Phase 2 syscalls: symlink/readlink -----------------------------------

[[nodiscard]] uint32_t sys_symlink(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    (void)frame;
    // ext2 mutation layer does not yet support symlinks
    return kErrnoNoSys;
}

[[nodiscard]] uint32_t sys_readlink(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    (void)frame;
    return kErrnoInvalid;
}

// -- Phase 2 syscalls: file descriptor ops --------------------------------

[[nodiscard]] uint32_t sys_fchdir(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    (void)frame;
    // Single-directory model, always /
    return kErrnoNoSys;
}

[[nodiscard]] uint32_t sys_fchmod(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    (void)frame;
    // Permissions are not enforced, succeed silently
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

// -- Phase 2 syscalls: vectored I/O ---------------------------------------

struct IoVec32 {
    uint32_t base;
    uint32_t length;
};

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
                    block_current_process_until_rescheduled(process, true, 0U);
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
                if (block_current_process_until_rescheduled(process, true, 0U)) {
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
                if (block_current_process_until_rescheduled(process, false, 0U)) {
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

// -- Phase 2 syscalls: select/poll ----------------------------------------

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
        block_current_process_until_rescheduled(process, true,
                                                g_scheduler_ticks + 1U);
        ++elapsed;
    }
}

struct PollFd32 {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

constexpr int16_t kPollIn = 0x0001;
constexpr int16_t kPollOut = 0x0004;
constexpr int16_t kPollNVal = 0x0020;

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
            block_current_process_until_rescheduled(process, false, wake);
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

        block_current_process_until_rescheduled(process, true,
                                                g_scheduler_ticks + 1U);
        ++elapsed;
    }
}

// -- Phase 2 syscalls: nanosleep (proper implementation) ------------------

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
        if (block_current_process_until_rescheduled(process, false, target_tick)) {
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

// -- Signal delivery framework --------------------------------------------

void init_signal_state(Process* process) noexcept {
    if (process == nullptr) {
        return;
    }
    zero_region(reinterpret_cast<uint8_t*>(&process->signals),
                static_cast<uint32_t>(sizeof(process->signals)));
    // SIGCHLD defaults to ignore
    process->signals.handlers[kSigChld].handler = kSigIgn;
}

bool is_default_terminate(uint32_t signum) noexcept {
    // Signals whose default action is to terminate
    switch (signum) {
    case kSigHup: case kSigInt: case kSigQuit: case kSigIll:
    case kSigAbrt: case kSigKill: case kSigSegv: case kSigPipe:
    case kSigAlrm: case kSigTerm:
        return true;
    default:
        return false;
    }
}

bool is_default_ignore(uint32_t signum) noexcept {
    switch (signum) {
    case kSigChld: case kSigCont:
        return true;
    default:
        return false;
    }
}

void send_signal_to_process(Process* target, uint32_t signum) noexcept {
    if (target == nullptr || signum == 0U || signum >= kMaxSignals) {
        return;
    }
    target->signals.pending |= (1U << signum);
    // Wake up waiting processes so they can handle the signal
    if (target->state == ProcessState::Waiting) {
        target->state = ProcessState::Runnable;
        target->waiting_for_console_input = false;
        target->wake_tick = 0U;
    }
}

// Push a signal frame onto the user stack and redirect execution to the handler.
// Returns true if a signal was delivered (context was modified).
bool deliver_one_signal(Process* process) noexcept {
    if (process == nullptr || process->signals.in_handler) {
        return false;
    }

    const uint32_t deliverable = process->signals.pending & ~process->signals.blocked;
    if (deliverable == 0U) {
        return false;
    }

    // Find lowest-numbered pending unblocked signal
    uint32_t signum = 0U;
    for (uint32_t i = 1U; i < kMaxSignals; ++i) {
        if ((deliverable & (1U << i)) != 0U) {
            signum = i;
            break;
        }
    }
    if (signum == 0U) {
        return false;
    }

    // Clear pending bit
    process->signals.pending &= ~(1U << signum);

    const SignalHandler32& handler = process->signals.handlers[signum];

    // Handle SIG_IGN
    if (handler.handler == kSigIgn) {
        return false;
    }

    // Handle SIG_DFL
    if (handler.handler == kSigDfl) {
        if (is_default_ignore(signum)) {
            return false;
        }
        if (signum == kSigStop || signum == kSigTstp) {
            process->state = ProcessState::Stopped;
            process->exit_status = (signum << 8U) | 0x7FU; // WIFSTOPPED encoding
            // Notify parent of stopped child
            Process* parent = find_process(process->ppid);
            if (parent != nullptr) {
                send_signal_to_process(parent, kSigChld);
                if (parent->state == ProcessState::Waiting) {
                    resume_waiting_parent(parent);
                }
            }
            return true; // Context modified (process stopped)
        }
        if (signum == kSigCont) {
            if (process->state == ProcessState::Stopped) {
                process->state = ProcessState::Runnable;
            }
            return false;
        }
        if (is_default_terminate(signum)) {
            // Default action: terminate
            process->exit_status = 128U + signum;
            process->state = ProcessState::Exited;
            return true; // Context is irrelevant, process is dying
        }
        return false;
    }

    // User handler -- push signal frame onto user stack
    const uint32_t frame_size = static_cast<uint32_t>(sizeof(SignalFrame32));
    uint32_t new_esp = process->context.esp - frame_size;
    new_esp = align_down(new_esp, 4U);

    uint8_t* frame_dest = nullptr;
    if (!translate_user_region(process, new_esp, frame_size, &frame_dest)) {
        // Can't push frame -- kill process
        process->exit_status = 128U + signum;
        process->state = ProcessState::Exited;
        return true;
    }

    auto* sig_frame = reinterpret_cast<SignalFrame32*>(frame_dest);

    // Write sigreturn trampoline code (7 bytes, padded to 8):
    //   B8 xx xx xx xx   movl $SYS_rt_sigreturn, %eax  (5 bytes)
    //   CD 80            int $0x80                      (2 bytes)
    //   90               nop                            (1 byte padding)
    {
        auto* code = reinterpret_cast<uint8_t*>(sig_frame->sigreturn_trampoline);
        code[0] = 0xB8U; // movl $imm32, %eax
        code[1] = static_cast<uint8_t>(SYS_rt_sigreturn & 0xFFU);
        code[2] = static_cast<uint8_t>((SYS_rt_sigreturn >> 8U) & 0xFFU);
        code[3] = 0U;
        code[4] = 0U;
        code[5] = 0xCDU; // int $0x80
        code[6] = 0x80U;
        code[7] = 0x90U; // nop
    }

    sig_frame->signum = signum;
    sig_frame->saved_context = process->context;
    sig_frame->saved_mask = process->signals.blocked;

    // Block signals during handler execution
    process->signals.saved_mask = process->signals.blocked;
    process->signals.blocked |= handler.mask | (1U << signum);
    if ((handler.flags & kSaNodefer) != 0U) {
        process->signals.blocked &= ~(1U << signum);
    }
    process->signals.in_handler = true;

    // Reset handler to SIG_DFL if SA_RESETHAND
    if ((handler.flags & kSaResethand) != 0U) {
        process->signals.handlers[signum].handler = kSigDfl;
        process->signals.handlers[signum].flags = 0U;
    }

    // Redirect user context to signal handler.
    // Push signum as argument on stack, trampoline as return address.
    const uint32_t trampoline_addr = new_esp; // Address of the trampoline code on stack
    new_esp -= 4U; // Space for signum argument
    static_cast<void>(write_user_u32(process, new_esp, signum));
    new_esp -= 4U; // Space for return address (trampoline)
    static_cast<void>(write_user_u32(process, new_esp, trampoline_addr));

    process->context.eip = handler.handler;
    process->context.esp = new_esp;

    return true;
}

// Sigaction syscall: register signal handlers
// ebx = signum, ecx = new sigaction ptr, edx = old sigaction ptr
struct SigAction32User {
    uint32_t sa_handler;
    uint32_t sa_flags;
    uint32_t sa_restorer;
    uint32_t sa_mask;
};

[[nodiscard]] uint32_t sys_sigaction_impl(Process* process, RegisterFrame* frame) noexcept {
    const uint32_t signum = frame->ebx;
    const uint32_t new_act_addr = frame->ecx;
    const uint32_t old_act_addr = frame->edx;

    if (signum == 0U || signum >= kMaxSignals || signum == kSigKill || signum == kSigStop) {
        return kErrnoInvalid;
    }

    // Return old action if requested
    if (old_act_addr != 0U) {
        SigAction32User old_act{};
        old_act.sa_handler = process->signals.handlers[signum].handler;
        old_act.sa_flags = process->signals.handlers[signum].flags;
        old_act.sa_restorer = process->signals.handlers[signum].restorer;
        old_act.sa_mask = process->signals.handlers[signum].mask;
        if (!write_user_bytes(process, old_act_addr, &old_act,
                              static_cast<uint32_t>(sizeof(old_act)))) {
            return kErrnoFault;
        }
    }

    // Set new action if provided
    if (new_act_addr != 0U) {
        uint8_t* raw = nullptr;
        if (!translate_user_region(process, new_act_addr,
                                   static_cast<uint32_t>(sizeof(SigAction32User)), &raw)) {
            return kErrnoFault;
        }
        const auto* new_act = reinterpret_cast<const SigAction32User*>(raw);
        process->signals.handlers[signum].handler = new_act->sa_handler;
        process->signals.handlers[signum].flags = new_act->sa_flags;
        process->signals.handlers[signum].restorer = new_act->sa_restorer;
        process->signals.handlers[signum].mask = new_act->sa_mask;
    }

    return 0U;
}

// kill syscall: send signal to process
[[nodiscard]] uint32_t sys_kill_impl(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    const int32_t target_pid = static_cast<int32_t>(frame->ebx);
    const uint32_t signum = frame->ecx;

    if (signum >= kMaxSignals) {
        return kErrnoInvalid;
    }

    // Signal 0 is a validity check, don't actually send
    if (signum == 0U) {
        if (target_pid > 0) {
            return find_process(static_cast<uint32_t>(target_pid)) != nullptr ? 0U : kErrnoNoSys;
        }
        return 0U;
    }

    if (target_pid > 0) {
        // Send to specific process
        Process* target = find_process(static_cast<uint32_t>(target_pid));
        if (target == nullptr) {
            return kErrnoNoSys; // ESRCH
        }
        send_signal_to_process(target, signum);
    } else if (target_pid == 0 || target_pid == -1) {
        // Send to all processes in group (simplified: send to all)
        for (auto& proc : g_processes) {
            if (proc.in_use && proc.state != ProcessState::Exited) {
                send_signal_to_process(&proc, signum);
            }
        }
    } else {
        // Send to specific process group (simplified: same as pid)
        Process* target = find_process(static_cast<uint32_t>(-target_pid));
        if (target == nullptr) {
            return kErrnoNoSys;
        }
        send_signal_to_process(target, signum);
    }

    return 0U;
}

// sigreturn: restore context from signal frame on user stack
[[noreturn]] void sys_rt_sigreturn_impl(Process* process, RegisterFrame* frame) noexcept {
    (void)frame;
    // The signal frame is at esp (after the trampoline's int $0x80 popped the
    // return address). Walk up to find it.
    // When the trampoline executes:
    //   movl $SYS_rt_sigreturn, %eax
    //   int $0x80
    // At this point esp points past the return address and signum argument,
    // so the frame starts at esp - 8 (return addr + signum) at the base.
    // Actually, the trampoline IS the start of the signal frame.
    // The stack at int $0x80 time:
    //   [return addr to trampoline] [signum arg] [trampoline code...]
    // After the handler returns, ESP points to [signum arg].
    // After trampoline pops nothing (it's code, not stack), ESP is unchanged.
    // We need to find the SignalFrame32 on the stack.
    //
    // Frame layout on stack (low to high):
    //   return_addr (trampoline addr)  <- was ESP when handler called
    //   signum_arg
    //   [signal frame begins here]:
    //     sigreturn_trampoline[2]
    //     signum
    //     saved_context
    //     saved_mask
    //
    // When sigreturn is called, ESP is at the point after handler's 'ret'
    // popped the return address (trampoline addr). So ESP now points to
    // signum_arg. The signal frame is at ESP + 4 (past the signum arg).

    const uint32_t frame_addr = process->context.esp + 4U;
    uint8_t* raw = nullptr;
    if (!translate_user_region(process, frame_addr,
                               static_cast<uint32_t>(sizeof(SignalFrame32)), &raw)) {
        // Can't restore -- kill process and dispatch next
        process->exit_status = 128U + kSigSegv;
        process->state = ProcessState::Exited;
        dispatch_next_runnable("sigreturn failed to read signal frame");
        __builtin_unreachable();
    }

    const auto* sig_frame = reinterpret_cast<const SignalFrame32*>(raw);

    // Restore context
    process->context = sig_frame->saved_context;
    process->signals.blocked = sig_frame->saved_mask;
    process->signals.in_handler = false;

    // Must resume via the restored context, NOT the normal syscall return.
    // The normal iret would go back to the trampoline (where int $0x80 was
    // called), but we need to go back to the original pre-signal location.
    activate_process(process);
    i486_resume_user_context(&process->context);
    __builtin_unreachable();
}

// signal() syscall (simplified SIG_DFL/SIG_IGN/handler)
[[nodiscard]] uint32_t sys_signal_impl(Process* process, RegisterFrame* frame) noexcept {
    const uint32_t signum = frame->ebx;
    const uint32_t handler = frame->ecx;

    if (signum == 0U || signum >= kMaxSignals || signum == kSigKill || signum == kSigStop) {
        return kErrnoInvalid;
    }

    const uint32_t old_handler = process->signals.handlers[signum].handler;
    process->signals.handlers[signum].handler = handler;
    process->signals.handlers[signum].flags = kSaRestart;
    process->signals.handlers[signum].mask = 0U;
    process->signals.handlers[signum].restorer = 0U;
    return old_handler;
}

uint32_t dispatch_syscall(Process* process, RegisterFrame* frame) noexcept;

extern "C" uint32_t i486_handle_syscall(RegisterFrame* frame) noexcept {
    Process* process = g_current_process;
    if (frame == nullptr || process == nullptr) {
        return static_cast<uint32_t>(-1);
    }
    return dispatch_syscall(process, frame);
}

uint32_t dispatch_syscall(Process* process, RegisterFrame* frame) noexcept {
    process->context = capture_user_context(frame);

    switch (frame->eax) {
    case SYS_read:
        return sys_read(process, frame);
    case SYS_write:
        return sys_write(process, frame);
    case SYS_open:
        return sys_open(process, frame);
    case SYS_close: {
        const int user_fd = static_cast<int>(frame->ebx);
        if (user_fd < 0 || user_fd >= 32) {
            return kErrnoBadF;
        }
        const int global_slot = process->fd_map[user_fd];
        if (global_slot < 0) {
            return kErrnoBadF;
        }
        process->fd_map[user_fd] = -1;
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
    case SYS_link:
        return kErrnoNoSys; // Hard links not supported
    case SYS_chmod:
    case SYS_chown:
        return 0U; // Single-user model, succeed silently
    case SYS_setuid:
    case SYS_setgid:
        return 0U; // Single-user model
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
    case SYS_geteuid:
    case SYS_getgid:
    case SYS_getegid:
        return 0U;
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
    case SYS_setreuid:
    case SYS_setregid:
        return 0U;
    case SYS_rt_sigprocmask:
        return sys_rt_sigprocmask_compat(process, frame);
    case SYS_getpgid: {
        // getpgid(0) returns caller's pgid; getpgid(pid) returns that process's pgid
        const uint32_t target = frame->ebx;
        if (target == 0U) {
            return process->pgid;
        }
        Process* target_proc = find_process(target);
        return target_proc != nullptr ? target_proc->pgid : kErrnoNoSys;
    }
    case SYS_getdents:
        return sys_getdents(process, frame);
    case SYS_mremap:
        return kErrnoNoSys;
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
        if (frame->ecx != 0U && !translate_user_region(process, frame->ecx, 16U, &addr_raw))
            return kErrnoFault;
        return static_cast<uint32_t>(ksocket::sys_bind(
            static_cast<int>(frame->ebx),
            reinterpret_cast<const ksocket::SockAddrIn*>(addr_raw)));
    }
    case SYS_connect: {
        uint8_t* addr_raw = nullptr;
        if (frame->ecx != 0U && !translate_user_region(process, frame->ecx, 16U, &addr_raw))
            return kErrnoFault;
        return static_cast<uint32_t>(ksocket::sys_connect(
            static_cast<int>(frame->ebx),
            reinterpret_cast<const ksocket::SockAddrIn*>(addr_raw)));
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
        if (!translate_user_region(process, frame->ecx, frame->edx, &buf_raw))
            return kErrnoFault;
        uint8_t* addr_raw = nullptr;
        if (frame->edi != 0U) static_cast<void>(translate_user_region(process, frame->edi, 16U, &addr_raw));
        return static_cast<uint32_t>(ksocket::sys_sendto(
            static_cast<int>(frame->ebx), buf_raw, frame->edx,
            reinterpret_cast<const ksocket::SockAddrIn*>(addr_raw)));
    }
    case SYS_recvfrom: {
        uint8_t* buf_raw = nullptr;
        if (!translate_user_region(process, frame->ecx, frame->edx, &buf_raw))
            return kErrnoFault;
        return static_cast<uint32_t>(ksocket::sys_recvfrom(
            static_cast<int>(frame->ebx), buf_raw, frame->edx, nullptr));
    }
    case SYS_shutdown:
        return static_cast<uint32_t>(ksocket::sys_shutdown(
            static_cast<int>(frame->ebx),
            static_cast<int>(frame->ecx)));
    case SYS_setsockopt:
    case SYS_getsockopt:
    case SYS_getsockname:
    case SYS_getpeername:
    case SYS_sendmsg:
    case SYS_recvmsg:
    case SYS_socketpair:
        return kErrnoNoSys; // Not yet implemented
    // Phase 3 POSIX completeness syscalls
    case SYS_gettid:
        return process->pid; // No threads, tid == pid
    case SYS_sched_yield:
        process->ticks_remaining = 0U;
        return 0U;
    case SYS_lstat:
        return sys_stat(process, frame); // No symlinks, lstat == stat
    case SYS_dup3: {
        const int old_user_fd = static_cast<int>(frame->ebx);
        const int new_user_fd = static_cast<int>(frame->ecx);
        const uint32_t flags = frame->edx;
        if (new_user_fd < 0 || new_user_fd >= 32) {
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
        if ((flags & 0x80000U) != 0U) { // O_CLOEXEC
            static_cast<void>(bootfs::set_descriptor_flags(global_slot, 1));
        }
        return static_cast<uint32_t>(new_user_fd);
    }
    case SYS_pipe2: {
        int pipe_fds[2] = {-1, -1};
        const int result = bootfs::make_pipe(pipe_fds);
        if (result != 0) {
            return kErrnoNoMem;
        }
        const uint32_t flags = frame->ecx;
        if ((flags & 0x80000U) != 0U) { // O_CLOEXEC
            static_cast<void>(bootfs::set_descriptor_flags(pipe_fds[0], 1));
            static_cast<void>(bootfs::set_descriptor_flags(pipe_fds[1], 1));
        }
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
    case SYS_fsync:
    case SYS_fdatasync: {
        const int gfd = resolve_fd(process, static_cast<int>(frame->ebx));
        return (gfd >= 0 && bootfs::is_open(gfd)) ? 0U : kErrnoBadF;
    }
    case SYS_flock: {
        const int gfd = resolve_fd(process, static_cast<int>(frame->ebx));
        return (gfd >= 0 && bootfs::is_open(gfd)) ? 0U : kErrnoBadF;
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
            block_current_process_until_rescheduled(process, false, 0U);
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
            if (!proc.in_use) continue;
            if (offset + sizeof(ProcInfoEntry) > buf_size) break;
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

} // namespace

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
    // Init process is the session leader with the console as ctty
    shell_process->ctty_slot = 0; // Global slot 0 = stdin console

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
        return false;
    }
    if (!prepare_supervised_service_process(init_service, shell_process)) {
        return false;
    }

    Process* hold_process = nullptr;
    SupervisedService* hold_service = register_optional_support_services(init_service, &hold_process);
    if (hold_service == nullptr || hold_process == nullptr) {
        return false;
    }

    initialize_legacy_pic();
    initialize_pit(kTimerHz);
    initialize_realtime_clock();
    ext2_reader::set_timestamp_provider(current_epoch_seconds);

    console::write_string("Launching supervised Ring 3 services under timer scheduler");
    console::newline();
    dispatch_next_runnable("failed to select initial supervised i486 service");
}

} // namespace xinim::i486::ring3
