#include "ring3.hpp"

#include "bootfs.hpp"
#include "console.hpp"
#include "elf32_loader.hpp"
#include "shell.hpp"
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
constexpr uint8_t kSyscallVector = 0x80U;
constexpr uint32_t kKernelStackSize = 8192U;
constexpr size_t kMaxProcesses = 2U;
constexpr uint32_t kUserEflags = 0x202U;
constexpr uint32_t kMaxExecArgs = 16U;
constexpr uint32_t kMaxExecEnvs = 16U;
constexpr uint32_t kMaxExecStringBytes = 512U;
constexpr uint32_t kWaitNoHang = 1U;
constexpr int32_t kWaitPidAny = -1;

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

enum class ProcessState : uint8_t {
    Empty = 0,
    Runnable = 1,
    Waiting = 2,
    Exited = 3,
};

struct Process {
    bool in_use;
    uint32_t pid;
    uint32_t ppid;
    ProcessState state;
    uint32_t exit_status;
    uint32_t saved_kernel_esp;
    uint32_t segment_base;
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

alignas(16) GdtEntry g_gdt[7]{};
alignas(16) TssEntry g_tss{};
alignas(16) IdtEntry g_idt[256]{};
alignas(16) uint8_t g_bootstrap_kernel_stack[kKernelStackSize]{};
alignas(16) Process g_processes[kMaxProcesses]{};
const xinim::boot::BootInfo* g_boot_info = nullptr;
Process* g_current_process = nullptr;
Process* g_shell_process = nullptr;
uint32_t g_next_pid = 1U;

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

[[nodiscard]] uint32_t align_down(uint32_t value, uint32_t alignment) noexcept {
    return value & ~(alignment - 1U);
}

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
    const uint32_t limit = elf32::kUserAddressSpaceSize - 1U;
    set_gdt_entry(3, base, limit, 0xFAU, 0x40U);
    set_gdt_entry(4, base, limit, 0xF2U, 0x40U);
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

void mask_legacy_pic() noexcept {
    outb(0x21U, 0xFFU);
    outb(0xA1U, 0xFFU);
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
            process.segment_base = compute_segment_base(process);
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

[[nodiscard]] Process* find_waiting_child(Process* parent, int32_t requested_pid) noexcept {
    if (parent == nullptr) {
        return nullptr;
    }
    for (auto& process : g_processes) {
        if (!process.in_use || process.ppid != parent->pid) {
            continue;
        }
        if (process.state != ProcessState::Exited) {
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
    return true;
}

[[noreturn]] void resume_waiting_parent(Process* parent) noexcept {
    activate_process(parent);
    i486_resume_saved_kernel_stack(parent->saved_kernel_esp);
    for (;;) {
        asm volatile("cli; hlt");
    }
}

[[noreturn]] void exit_current_process(uint32_t exit_status, const char* fallback_reason) noexcept {
    Process* process = g_current_process;
    if (process == nullptr) {
        resume_rescue_shell(fallback_reason);
    }

    process->exit_status = exit_status;
    process->state = ProcessState::Exited;

    Process* parent = find_process(process->ppid);
    if (parent != nullptr && parent->state == ProcessState::Waiting) {
        resume_waiting_parent(parent);
    }

    if (process == g_shell_process) {
        resume_rescue_shell("xash exited; entering rescue shell");
    }

    resume_rescue_shell(fallback_reason);
}

[[nodiscard]] uint32_t wait_status_for_exit(uint32_t exit_status) noexcept {
    return (exit_status & 0xFFU) << 8U;
}

[[nodiscard]] uint32_t sys_read(Process* process, RegisterFrame* frame) noexcept {
    const int fd = static_cast<int>(frame->ebx);
    const uint32_t count = frame->edx;
    if (count == 0U) {
        return 0U;
    }

    uint8_t* buffer = nullptr;
    if (!translate_user_region(process, frame->ecx, count, &buffer)) {
        return static_cast<uint32_t>(-1);
    }

    if (fd == 0) {
        uint32_t written = 0U;
        while (written < count) {
            buffer[written] = static_cast<uint8_t>(console::debug_read_char());
            ++written;
            if (buffer[written - 1U] == '\r' || buffer[written - 1U] == '\n') {
                break;
            }
        }
        return written;
    }

    return static_cast<uint32_t>(bootfs::read(fd, buffer, count));
}

[[nodiscard]] uint32_t sys_write(Process* process, RegisterFrame* frame) noexcept {
    const int fd = static_cast<int>(frame->ebx);
    const uint32_t count = frame->edx;
    if (fd == 1 || fd == 2) {
        const uint8_t* buffer = nullptr;
        if (!translate_user_region(process,
                                   frame->ecx,
                                   count,
                                   const_cast<uint8_t**>(&buffer))) {
            return static_cast<uint32_t>(-1);
        }
        for (uint32_t index = 0U; index < count; ++index) {
            console::debug_write_char(static_cast<char>(buffer[index]));
        }
        return count;
    }

    uint8_t* buffer = nullptr;
    if (!translate_user_region(process, frame->ecx, count, &buffer)) {
        return static_cast<uint32_t>(-1);
    }

    const int result = bootfs::write(fd, buffer, count);
    return result >= 0 ? static_cast<uint32_t>(result) : static_cast<uint32_t>(-1);
}

[[nodiscard]] uint32_t sys_open(Process* process, RegisterFrame* frame) noexcept {
    char path[128]{};
    if (!copy_user_string(process, frame->ebx, path, sizeof(path))) {
        return static_cast<uint32_t>(-1);
    }
    return static_cast<uint32_t>(bootfs::open(path, frame->ecx, frame->edx));
}

[[nodiscard]] uint32_t sys_access(Process* process, RegisterFrame* frame) noexcept {
    char path[128]{};
    if (!copy_user_string(process, frame->ebx, path, sizeof(path))) {
        return static_cast<uint32_t>(-1);
    }
    return static_cast<uint32_t>(bootfs::access(path));
}

[[nodiscard]] uint32_t sys_chdir(Process* process, RegisterFrame* frame) noexcept {
    char path[64]{};
    if (!copy_user_string(process, frame->ebx, path, sizeof(path))) {
        return static_cast<uint32_t>(-1);
    }
    return string_equals(path, "/") ? 0U : static_cast<uint32_t>(-1);
}

[[nodiscard]] uint32_t sys_getcwd(Process* process, RegisterFrame* frame) noexcept {
    uint8_t* buffer = nullptr;
    if (!translate_user_region(process, frame->ebx, frame->ecx, &buffer) || frame->ecx < 2U) {
        return static_cast<uint32_t>(-1);
    }
    buffer[0] = '/';
    buffer[1] = '\0';
    return 1U;
}

[[nodiscard]] uint32_t sys_fork(Process* process, RegisterFrame* frame) noexcept {
    Process* child = allocate_process(process->pid);
    if (child == nullptr) {
        return static_cast<uint32_t>(-1);
    }
    copy_region(child->address_space, process->address_space, elf32::kUserAddressSpaceSize);
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
        process->context.eax = static_cast<uint32_t>(-1);
        activate_process(process);
        i486_resume_user_context(&process->context);
        __builtin_unreachable();
    }
    if (!copy_exec_vector(process, frame->ecx, &argv, path) ||
        !copy_exec_vector(process, frame->edx, &envp)) {
        process->context = capture_user_context(frame);
        process->context.eax = static_cast<uint32_t>(-1);
        activate_process(process);
        i486_resume_user_context(&process->context);
        __builtin_unreachable();
    }

    const bootfs::FileRecord* file = bootfs::find(path);
    if (file == nullptr || !file->executable || !load_process_image(process, file, argv, envp)) {
        process->context = capture_user_context(frame);
        process->context.eax = static_cast<uint32_t>(-1);
        activate_process(process);
        i486_resume_user_context(&process->context);
        __builtin_unreachable();
    }

    activate_process(process);
    i486_resume_user_context(&process->context);
    __builtin_unreachable();
}

[[nodiscard]] uint32_t sys_wait4(Process* process, RegisterFrame* frame) noexcept {
    const int32_t requested_pid = static_cast<int32_t>(frame->ebx);
    const uint32_t options = frame->edx;

    if (requested_pid == 0 || requested_pid < kWaitPidAny) {
        return static_cast<uint32_t>(-1);
    }

    const bool no_hang = (options & kWaitNoHang) != 0U;

    if (!has_child(process)) {
        return static_cast<uint32_t>(-1);
    }

    if (requested_pid > 0 && find_child(process, requested_pid) == nullptr) {
        return static_cast<uint32_t>(-1);
    }

    Process* child = find_waiting_child(process, requested_pid);
    if (child == nullptr) {
        if (no_hang) {
            return 0U;
        }

        Process* runnable_child = find_child(process, requested_pid);
        if (runnable_child == nullptr) {
            return static_cast<uint32_t>(-1);
        }
        process->state = ProcessState::Waiting;
        activate_process(runnable_child);
        i486_switch_to_user_context(&runnable_child->context, &process->saved_kernel_esp);
        process->state = ProcessState::Runnable;
        activate_process(process);

        child = find_waiting_child(process, requested_pid);
        if (child == nullptr) {
            return static_cast<uint32_t>(-1);
        }
    }

    if (child->state != ProcessState::Exited) {
        process->state = ProcessState::Waiting;
        activate_process(child);
        i486_switch_to_user_context(&child->context, &process->saved_kernel_esp);
        process->state = ProcessState::Runnable;
        activate_process(process);
        child = find_waiting_child(process, requested_pid);
        if (child == nullptr) {
            return static_cast<uint32_t>(-1);
        }
    }

    if (frame->ecx != 0U) {
        if (!write_user_u32(process, frame->ecx, wait_status_for_exit(child->exit_status))) {
            return static_cast<uint32_t>(-1);
        }
    }
    const uint32_t pid = child->pid;
    destroy_process(child);
    return pid;
}

extern "C" uint32_t i486_handle_syscall(RegisterFrame* frame) noexcept {
    Process* process = g_current_process;
    if (frame == nullptr || process == nullptr) {
        return static_cast<uint32_t>(-1);
    }

    process->context = capture_user_context(frame);

    switch (frame->eax) {
    case SYS_read:
        return sys_read(process, frame);
    case SYS_write:
        return sys_write(process, frame);
    case SYS_open:
        return sys_open(process, frame);
    case SYS_close:
        return static_cast<uint32_t>(bootfs::close(static_cast<int>(frame->ebx)));
    case SYS_access:
        return sys_access(process, frame);
    case SYS_chdir:
        return sys_chdir(process, frame);
    case SYS_getcwd:
        return sys_getcwd(process, frame);
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
    case SYS_stat:
    case SYS_fstat:
    case SYS_dup:
    case SYS_dup2:
    case SYS_pipe:
        return static_cast<uint32_t>(-1);
    case SYS_ioctl:
        return 0U;
    case SYS_brk:
        return 0U;
    case SYS_getuid:
    case SYS_geteuid:
    case SYS_getgid:
    case SYS_getegid:
        return 0U;
    default:
        console::write_string("Unhandled i486 syscall");
        console::newline();
        return static_cast<uint32_t>(-1);
    }
}

[[noreturn]] void handle_fault(uint32_t vector, uint32_t error_code) noexcept {
    console::write_string("i486 fault vector=");
    console::write_dec32(vector);
    console::write_string(" error=");
    console::write_hex32(error_code);
    if (vector == 14U) {
        uint32_t fault_address = 0U;
        asm volatile("mov %%cr2, %0" : "=r"(fault_address));
        console::write_string(" cr2=");
        console::write_hex32(fault_address);
    }
    console::newline();

    if (g_current_process != nullptr && g_current_process != g_shell_process) {
        exit_current_process(128U + vector, "child user process faulted");
    }

    resume_rescue_shell("Ring 3 xash faulted");
}

extern "C" [[noreturn]] void i486_handle_fault(uint32_t vector,
                                                uint32_t error_code) noexcept {
    handle_fault(vector, error_code);
}

} // namespace

bool launch_xash(const xinim::boot::BootInfo& info) noexcept {
    g_boot_info = &info;

    const bootfs::FileRecord* mksh = bootfs::find("/bin/mksh");
    const bootfs::FileRecord* shell = mksh;
    if (shell == nullptr || !shell->executable) {
        shell = bootfs::find("/bin/sh");
    }
    if (shell == nullptr || !shell->executable) {
        shell = bootfs::find("/bin/xash");
    }
    if (shell == nullptr || !shell->executable) {
        return false;
    }

    console::write_string("Seeding shell from Multiboot2 module");
    console::newline();

    initialize_protection();
    mask_legacy_pic();

    ExecVector<kMaxExecArgs> argv{};
    ExecVector<kMaxExecEnvs> envp{};
    const char* shell_path = "/bin/xash";
    const char* shell_env = "SHELL=/bin/xash";
    if (mksh != nullptr && mksh->executable) {
        shell_path = "/bin/mksh";
        shell_env = "SHELL=/bin/mksh";
    } else if (bootfs::find("/bin/sh") != nullptr) {
        shell_path = "/bin/sh";
        shell_env = "SHELL=/bin/sh";
    }

    if (!append_exec_string(&argv, shell_path) ||
        !append_exec_string(&envp, "PATH=/bin") ||
        !append_exec_string(&envp, shell_env)) {
        return false;
    }

    Process* shell_process = allocate_process(0U);
    if (shell_process == nullptr || !load_process_image(shell_process, shell, argv, envp)) {
        return false;
    }
    g_shell_process = shell_process;

    console::write_string("Launching Ring 3 shell");
    console::newline();

    activate_process(shell_process);
    i486_resume_user_context(&shell_process->context);
    __builtin_unreachable();
    return false;
}

} // namespace xinim::i486::ring3
