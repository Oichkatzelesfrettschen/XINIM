#include "sched.hpp"

#include "console.hpp"
#include "kutil.hpp"
#include "process.hpp"
#include "signal.hpp"
#include "tcp.hpp"
#include "tty.hpp"

namespace xinim::i486::ring3 {

uint32_t effective_quantum(const Process* process) noexcept {
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

void wake_ready_waiters() noexcept {
    const bool have_console_input = console::tty_has_input();
    const bool pipe_eof = bootfs::consume_pipe_eof_event();
    for (auto& process : g_processes) {
        if (!process.in_use || process.state != ProcessState::Waiting) {
            continue;
        }
        if ((process.wait_reason == WaitReason::ConsoleInput ||
             process.wait_reason == WaitReason::PipeIO) &&
            (have_console_input || pipe_eof)) {
            process.wait_reason = WaitReason::None;
            process.state = ProcessState::Runnable;
        }
        // Wake TcpConnect waiters when connection state changes
        if (process.wait_reason == WaitReason::TcpConnect) {
            const auto tcp_st = net::tcp_state(process.wait_tcp_conn);
            if (tcp_st == net::TcpState::Established ||
                tcp_st == net::TcpState::Closed) {
                process.wait_reason = WaitReason::None;
                process.state = ProcessState::Runnable;
            }
        }
        if (process.wake_tick != 0U && process.wake_tick <= g_scheduler_ticks) {
            process.wake_tick = 0U;
            process.state = ProcessState::Runnable;
        }
    }
}

Process* select_next_runnable(Process* preferred_current) noexcept {
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
    if (process->state == ProcessState::Runnable) {
        deliver_one_signal(process);
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

[[noreturn]] void dispatch_next_runnable(const char* rescue_reason) noexcept {
    wake_ready_waiters();
    Process* next = select_next_runnable(g_current_process);
    if (next == nullptr) {
        resume_rescue_shell(rescue_reason);
    }
    dispatch_process(next);
}

bool block_current_process_until_rescheduled(Process* process,
                                             WaitReason reason,
                                             uint64_t wake_tick) noexcept {
    if (process == nullptr) {
        resume_rescue_shell("attempted to block missing i486 process");
    }
    process->state = ProcessState::Waiting;
    process->wait_reason = reason;
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
    process->wait_reason = WaitReason::None;
    process->wake_tick = 0U;
    const bool has_signal = (process->signals.pending & ~process->signals.blocked) != 0U;
    activate_process(process);
    return has_signal;
}

extern "C" [[noreturn]] void i486_handle_timer_irq(RegisterFrame* frame) noexcept {
    Process* current = g_current_process;
    if (current != nullptr && current->in_use) {
        current->context = capture_user_context(frame);
    }

    ++g_scheduler_ticks;

    console::tty_poll_input();

    const uint32_t tty_signal = console::consume_pending_tty_signal();
    if (tty_signal != 0U && current != nullptr && current->in_use) {
        send_signal_to_process(current, tty_signal);
    }

    // Line discipline signals (ISIG: works for serial input too)
    const uint32_t ldisc_signal = tty::consume_pending_ldisc_signal();
    if (ldisc_signal != 0U && current != nullptr && current->in_use) {
        send_signal_to_process(current, ldisc_signal);
    }

    for (auto& proc : g_processes) {
        if (!proc.in_use) continue;

        // Legacy alarm(2)
        if (proc.alarm_tick != 0U && proc.alarm_tick <= g_scheduler_ticks) {
            proc.alarm_tick = 0U;
            send_signal_to_process(&proc, kSigAlrm);
        }

        // ITIMER_REAL -> SIGALRM
        if (proc.itimer_real.deadline != 0U &&
            proc.itimer_real.deadline <= g_scheduler_ticks) {
            send_signal_to_process(&proc, kSigAlrm);
            if (proc.itimer_real.interval != 0U) {
                proc.itimer_real.deadline = g_scheduler_ticks + proc.itimer_real.interval;
            } else {
                proc.itimer_real.deadline = 0U;
            }
        }

        // ITIMER_VIRTUAL -> SIGVTALRM (fires only when process is running)
        if (proc.itimer_virtual.deadline != 0U &&
            &proc == current &&
            proc.itimer_virtual.deadline <= g_scheduler_ticks) {
            send_signal_to_process(&proc, kSigVtalrm);
            if (proc.itimer_virtual.interval != 0U) {
                proc.itimer_virtual.deadline = g_scheduler_ticks + proc.itimer_virtual.interval;
            } else {
                proc.itimer_virtual.deadline = 0U;
            }
        }

        // ITIMER_PROF -> SIGPROF (fires when process is running or in syscall)
        if (proc.itimer_prof.deadline != 0U &&
            &proc == current &&
            proc.itimer_prof.deadline <= g_scheduler_ticks) {
            send_signal_to_process(&proc, kSigProf);
            if (proc.itimer_prof.interval != 0U) {
                proc.itimer_prof.deadline = g_scheduler_ticks + proc.itimer_prof.interval;
            } else {
                proc.itimer_prof.deadline = 0U;
            }
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
    outb(kPic1CommandPort, kPicEoi);
    if (next == nullptr) {
        resume_rescue_shell("timer interrupt found no runnable i486 process");
    }
    dispatch_process(next);
}

} // namespace xinim::i486::ring3
