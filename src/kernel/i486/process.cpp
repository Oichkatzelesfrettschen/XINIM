#include "process.hpp"

#include "console.hpp"
#include "hw_init.hpp"
#include "kutil.hpp"
#include "shell.hpp"
#include "signal.hpp"
#include "sched.hpp"

#ifdef XINIM_ARCH_I686
#include "../i686/sse.hpp"
#include "../i686/sysenter.hpp"
#endif

namespace xinim::i486::ring3 {

uint32_t compute_segment_base(const Process& process) noexcept {
    return static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(process.address_space) - elf32::kUserVirtualBase);
}

void activate_process(Process* process) noexcept {
    if (process == nullptr) {
        return;
    }
    g_current_process = process;
    set_user_segment_base(process->segment_base);
    const uint32_t kstack_top = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(process->kernel_stack + sizeof(process->kernel_stack)));
    g_tss.esp0 = kstack_top;
#ifdef XINIM_ARCH_I686
    // Keep SYSENTER_ESP (MSR 0x175) in sync with the current kernel stack so
    // SYSENTER lands on the right stack for this process.
    xinim::i686::sysenter::update_sysenter_esp(kstack_top);
#endif
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

Process* allocate_process(uint32_t parent_pid) noexcept {
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
            process.ctty_slot = -1;
            for (int fd_index = 0; fd_index < kMaxFds; ++fd_index) {
                process.fd_map[fd_index] = -1;
            }
            process.fd_map[0] = 0;
            process.fd_map[1] = 1;
            process.fd_map[2] = 2;
            bootfs::increment_slot_refcount(0);
            bootfs::increment_slot_refcount(1);
            bootfs::increment_slot_refcount(2);
            init_signal_state(&process);
#ifdef XINIM_ARCH_I686
            // Initialise FPU/SSE state image so the new process starts with
            // a clean x87 + MXCSR context (all exceptions masked, round-to-nearest).
            xinim::i686::sse::fpu_init_context(process.fxsave_buf);
#endif
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

Process* find_process(uint32_t pid) noexcept {
    for (auto& process : g_processes) {
        if (process.in_use && process.pid == pid) {
            return &process;
        }
    }
    return nullptr;
}

Process* find_child(Process* parent, int32_t requested_pid) noexcept {
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

bool has_child(Process* parent) noexcept {
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

int process_slot_index(const Process* process) noexcept {
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

void clear_saved_kernel_stack(Process* process) noexcept {
    if (process != nullptr) {
        process->saved_kernel_esp = 0U;
    }
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

} // namespace xinim::i486::ring3
