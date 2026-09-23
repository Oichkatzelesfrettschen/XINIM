#include "process.hpp"

#include "bootfs.hpp"
#include "console.hpp"
#include "hw_init.hpp"
#include "kutil.hpp"
#include "shell.hpp"
#include "signal.hpp"
#include "sched.hpp"
#include "user_backing.hpp"

#ifdef XINIM_ARCH_I686
#include "../i686/sse.hpp"
#include "../i686/sysenter.hpp"
#endif

namespace xinim::i486::ring3 {

namespace {
uint32_t g_console_session_id = 0U;
uint64_t g_console_session_generation = 0U;
uint64_t g_next_session_generation = 1U;

bool group_in_session(uint32_t pgid, const Process& session_member) noexcept {
    for (const auto& process : g_processes) {
        if (process.in_use && process.pgid == pgid &&
            process.session_id == session_member.session_id &&
            process.session_generation == session_member.session_generation) {
            return true;
        }
    }
    return false;
}

void signal_session_group(const Process& session_member, uint32_t pgid, uint32_t signal,
                          const Process* excluded) noexcept {
    for (auto& peer : g_processes) {
        if (peer.in_use && peer.state != ProcessState::Exited && &peer != excluded &&
            peer.session_id == session_member.session_id &&
            peer.session_generation == session_member.session_generation && peer.pgid == pgid) {
            send_signal_to_process(&peer, signal);
        }
    }
}
} // namespace

bool owns_controlling_terminal(const Process& process) noexcept {
    return process.has_controlling_terminal && process.session_id != 0U &&
           process.session_id == g_console_session_id &&
           process.session_generation == g_console_session_generation;
}

uint32_t acquire_controlling_terminal(Process& process) noexcept {
    if (process.session_id != process.pid ||
        (g_console_session_id != 0U &&
         (g_console_session_id != process.session_id ||
          g_console_session_generation != process.session_generation))) {
        return kErrnoPerm;
    }
    if (g_console_session_id == 0U) {
        g_console_session_id = process.session_id;
        g_console_session_generation = process.session_generation;
        bootfs::set_foreground_pgrp(static_cast<int>(process.pgid));
    }
    for (auto& member : g_processes) {
        if (member.in_use && member.session_id == process.session_id &&
            member.session_generation == process.session_generation) {
            member.has_controlling_terminal = true;
        }
    }
    return 0U;
}

void release_controlling_terminal(Process& process, bool continue_foreground) noexcept {
    if (owns_controlling_terminal(process) && process.pid == process.session_id) {
        const int foreground = bootfs::foreground_pgrp();
        if (foreground > 0) {
            const Process* excluded = continue_foreground ? nullptr : &process;
            signal_session_group(process, static_cast<uint32_t>(foreground),
                                 kSigHup, excluded);
            if (continue_foreground) {
                signal_session_group(process, static_cast<uint32_t>(foreground),
                                     kSigCont, excluded);
            }
        }
        for (auto& member : g_processes) {
            if (member.in_use && member.session_id == process.session_id &&
                member.session_generation == process.session_generation) {
                member.has_controlling_terminal = false;
            }
        }
        g_console_session_id = 0U;
        g_console_session_generation = 0U;
        bootfs::set_foreground_pgrp(0);
    }
    process.has_controlling_terminal = false;
}

void signal_foreground_terminal_group(uint32_t signum) noexcept {
    const int foreground = bootfs::foreground_pgrp();
    if (signum == 0U || signum >= kMaxSignals ||
        foreground <= 0 || g_console_session_id == 0U) {
        return;
    }
    for (auto& member : g_processes) {
        if (member.in_use && member.state != ProcessState::Exited &&
            member.pgid == static_cast<uint32_t>(foreground) &&
            owns_controlling_terminal(member)) {
            send_signal_to_process(&member, signum);
        }
    }
}

void initialize_supervised_session(Process& process, bool console_owner) noexcept {
    release_controlling_terminal(process);
    process.session_id = process.pid;
    // Supervised init retains PID 1 across reloads. A fresh generation keeps
    // surviving members of its previous session detached from the new terminal.
    process.session_generation = g_next_session_generation++;
    process.pgid = process.pid;
    process.executed_since_fork = false;
    if (console_owner) {
        static_cast<void>(acquire_controlling_terminal(process));
    }
}

void inherit_process_session(Process& child, const Process& parent) noexcept {
    child.session_id = parent.session_id;
    child.session_generation = parent.session_generation;
    child.pgid = parent.pgid;
    child.has_controlling_terminal = owns_controlling_terminal(parent);
    child.executed_since_fork = false;
}

uint32_t create_process_session(Process& process) noexcept {
    for (const auto& member : g_processes) {
        if (member.in_use && member.pgid == process.pid) {
            return kErrnoPerm;
        }
    }
    process.session_id = process.pid;
    process.session_generation = g_next_session_generation++;
    process.pgid = process.pid;
    process.has_controlling_terminal = false;
    return process.session_id;
}

uint32_t query_process_session(const Process& process, uint32_t pid) noexcept {
    if (pid == 0U) {
        return process.session_id;
    }
    const Process* target = find_process(pid);
    return target == nullptr ? kErrnoSrch : target->session_id;
}

uint32_t set_process_group(Process& caller, uint32_t pid, uint32_t pgid) noexcept {
    if (static_cast<int32_t>(pid) < 0 || static_cast<int32_t>(pgid) < 0) {
        return kErrnoInvalid;
    }
    const uint32_t target_pid = pid == 0U ? caller.pid : pid;
    Process* target = find_process(target_pid);
    if (target == nullptr || (target != &caller && target->ppid != caller.pid)) {
        return kErrnoSrch;
    }
    if (target != &caller && target->executed_since_fork) {
        return kErrnoAcces;
    }
    if (target->session_id != caller.session_id ||
        target->session_generation != caller.session_generation || target->session_id == target->pid) {
        return kErrnoPerm;
    }
    const uint32_t target_group = pgid == 0U ? target_pid : pgid;
    if (target_group != target_pid && !group_in_session(target_group, caller)) {
        return kErrnoPerm;
    }
    target->pgid = target_group;
    return 0U;
}

uint32_t set_terminal_foreground(Process& process, int32_t pgid) noexcept {
    if (!owns_controlling_terminal(process)) {
        return kErrnoNoTTY;
    }
    if (pgid <= 0) {
        return kErrnoInvalid;
    }
    if (!group_in_session(static_cast<uint32_t>(pgid), process)) {
        return kErrnoPerm;
    }
    const bool ignores_ttou = process.signals.handlers[kSigTtou].handler == kSigIgn;
    const bool blocks_ttou = (process.signals.blocked & (1U << kSigTtou)) != 0U;
    if (process.pgid != static_cast<uint32_t>(bootfs::foreground_pgrp()) &&
        !ignores_ttou && !blocks_ttou) {
        signal_session_group(process, process.pgid, kSigTtou, nullptr);
        return kErrnoIntr;
    }
    bootfs::set_foreground_pgrp(pgid);
    return 0U;
}

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
    uint32_t admitted = 0U;
    for (const auto &process : g_processes) {
        if (process.in_use) {
            ++admitted;
        }
    }
    const uint32_t capacity = user_backing::capacity_images();
    if (capacity < 2U || admitted >= capacity - 1U) {
        return nullptr;
    }
    for (auto& process : g_processes) {
        if (!process.in_use) {
            uint8_t* backing = user_backing::acquire();
            if (backing == nullptr) {
                return nullptr;
            }
            zero_region(reinterpret_cast<uint8_t*>(&process),
                        static_cast<uint32_t>(sizeof(process)));
            process.address_space = backing;
            process.in_use = true;
            process.pid = g_next_pid++;
            process.ppid = parent_pid;
            process.state = ProcessState::Runnable;
            process.file_creation_mask = 0022U;
            process.pgid = process.pid;
            process.session_id = process.pid;
            process.session_generation = g_next_session_generation++;
            process.cwd[0] = '/';
            process.cwd[1] = '\0';
            process.segment_base = compute_segment_base(process);
            process.has_controlling_terminal = false;
            reset_fd_map_to_console(&process);
            init_signal_state(&process);
#if defined(XINIM_ARCH_I686) && defined(XINIM_I686_FPU_CONTEXT_SWITCH)
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
    for (int fd_index = 0; fd_index < kMaxFds; ++fd_index) {
        const int slot = process->fd_map[fd_index];
        if (slot >= 0) {
            bootfs::decrement_slot_refcount(slot);
            process->fd_map[fd_index] = -1;
        }
    }
    uint8_t* backing = process->address_space;
    process->address_space = nullptr;
    if (backing != nullptr) {
        static_cast<void>(user_backing::release(backing));
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
