/**
 * @file signal.cpp
 * @brief Signal-related syscalls
 *
 * Implements POSIX signal syscalls: kill, sigaction, sigprocmask, sigreturn.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "../syscall_table.hpp"
#include "../signal.hpp"
#include "../uaccess.hpp"
#include "../pcb.hpp"
#include "../scheduler.hpp"
#include "../../early/serial_16550.hpp"
#include <cerrno>
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// sys_kill - Send signal to process
// ============================================================================

/**
 * @brief sys_kill - Send signal to process
 *
 * POSIX: int kill(pid_t pid, int sig)
 *
 * Special cases:
 * - pid > 0: Send to process with PID = pid
 * - pid == 0: Send to all processes in current process group
 * - pid == -1: Send to all processes (broadcast)
 * - pid < -1: Send to all processes in process group |pid|
 *
 * Week 10 Phase 2: Only pid > 0 supported
 * Week 11: Process groups
 *
 * @param pid Target process ID
 * @param sig Signal number
 * @return 0 on success, negative error on failure
 */
extern "C" int64_t sys_kill(uint64_t pid, uint64_t sig,
                            uint64_t, uint64_t, uint64_t, uint64_t) {
    char log_buf[128];

    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_kill(%lu, %lu)\n", pid, sig);
    early_serial.write(log_buf);

    // Validate signal number
    if (sig >= NSIG) {
        return -EINVAL;
    }

    // sig == 0 is a special case (check if process exists)
    if (sig == 0) {
        ProcessControlBlock* target = find_process_by_pid((xinim::pid_t)pid);
        if (!target) {
            return -ESRCH;  // No such process
        }
        return 0;  // Process exists
    }

    // Week 10 Phase 2: Only support pid > 0
    if (pid <= 0) {
        early_serial.write("[SYS_KILL] Process groups not yet supported (Week 11)\n");
        return -EINVAL;
    }

    // Find target process
    ProcessControlBlock* target = find_process_by_pid((xinim::pid_t)pid);
    if (!target) {
        return -ESRCH;  // No such process
    }

    // TODO Week 10 Phase 2: Check permissions
    // For now, allow all signals from any process
    // Week 11: Implement proper permission checking

    // Send signal
    int ret = send_signal(target, (int)sig);

    if (ret == 0) {
        std::snprintf(log_buf, sizeof(log_buf),
                      "[SYS_KILL] Sent signal %lu to process %lu\n",
                      sig, pid);
        early_serial.write(log_buf);
    }

    return ret;
}

// ============================================================================
// sys_sigaction - Set signal handler
// ============================================================================

/**
 * @brief sys_sigaction - Set signal action
 *
 * POSIX: int sigaction(int signum, const struct sigaction *act,
 *                      struct sigaction *oldact)
 *
 * @param signum Signal number
 * @param act_addr User pointer to new action (or NULL)
 * @param oldact_addr User pointer to old action (or NULL)
 * @return 0 on success, negative error on failure
 */
extern "C" int64_t sys_sigaction(uint64_t signum,
                                 uint64_t act_addr,
                                 uint64_t oldact_addr,
                                 uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Initialize signal state if needed
    if (!current->signal_state) {
        init_signal_state(current);
    }

    char log_buf[256];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_sigaction(%lu, 0x%lx, 0x%lx)\n",
                  signum, act_addr, oldact_addr);
    early_serial.write(log_buf);

    // Validate signal number
    if (signum < 1 || signum >= NSIG) {
        return -EINVAL;
    }

    // SIGKILL and SIGSTOP cannot be caught or ignored
    if (signum == SIGKILL || signum == SIGSTOP) {
        early_serial.write("[SIGACTION] Cannot catch SIGKILL or SIGSTOP\n");
        return -EINVAL;
    }

    SignalHandler* handler = &current->signal_state->handlers[signum];

    // Return old action if requested
    if (oldact_addr != 0) {
        sigaction_user oldact;
        oldact.sa_handler = handler->handler;
        oldact.sa_flags = handler->flags;
        oldact.sa_mask = handler->mask;
        oldact.sa_restorer = handler->restorer;

        int ret = copy_to_user(oldact_addr, &oldact, sizeof(sigaction_user));
        if (ret < 0) return ret;
    }

    // Set new action if provided
    if (act_addr != 0) {
        sigaction_user act;
        int ret = copy_from_user(&act, act_addr, sizeof(sigaction_user));
        if (ret < 0) return ret;

        handler->handler = act.sa_handler;
        handler->flags = act.sa_flags;
        handler->mask = act.sa_mask;
        handler->restorer = act.sa_restorer;

        // SIGKILL and SIGSTOP cannot be in the mask
        handler->mask &= ~((1ULL << SIGKILL) | (1ULL << SIGSTOP));

        std::snprintf(log_buf, sizeof(log_buf),
                      "[SIGACTION] Signal %lu: handler=0x%lx flags=0x%lx mask=0x%lx\n",
                      signum, handler->handler, handler->flags, handler->mask);
        early_serial.write(log_buf);
    }

    return 0;
}

// ============================================================================
// sys_sigprocmask - Manipulate signal mask
// ============================================================================

/**
 * @brief sys_sigprocmask - Change signal mask
 *
 * POSIX: int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
 *
 * @param how SIG_BLOCK, SIG_UNBLOCK, or SIG_SETMASK
 * @param set_addr User pointer to new mask (or NULL)
 * @param oldset_addr User pointer to old mask (or NULL)
 * @return 0 on success, negative error on failure
 */
extern "C" int64_t sys_sigprocmask(uint64_t how,
                                   uint64_t set_addr,
                                   uint64_t oldset_addr,
                                   uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Initialize signal state if needed
    if (!current->signal_state) {
        init_signal_state(current);
    }

    char log_buf[256];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_sigprocmask(%lu, 0x%lx, 0x%lx)\n",
                  how, set_addr, oldset_addr);
    early_serial.write(log_buf);

    // Return old mask if requested
    if (oldset_addr != 0) {
        uint64_t oldmask = current->signal_state->blocked;
        int ret = copy_to_user(oldset_addr, &oldmask, sizeof(uint64_t));
        if (ret < 0) return ret;

        std::snprintf(log_buf, sizeof(log_buf),
                      "[SIGPROCMASK] Old mask: 0x%lx\n", oldmask);
        early_serial.write(log_buf);
    }

    // Set new mask if provided
    if (set_addr != 0) {
        uint64_t newmask;
        int ret = copy_from_user(&newmask, set_addr, sizeof(uint64_t));
        if (ret < 0) return ret;

        // SIGKILL and SIGSTOP cannot be blocked
        newmask &= ~((1ULL << SIGKILL) | (1ULL << SIGSTOP));

        switch (how) {
            case SIG_BLOCK:
                // Add signals to blocked set
                current->signal_state->blocked |= newmask;
                std::snprintf(log_buf, sizeof(log_buf),
                              "[SIGPROCMASK] Blocked additional signals: 0x%lx\n",
                              newmask);
                early_serial.write(log_buf);
                break;

            case SIG_UNBLOCK:
                // Remove signals from blocked set
                current->signal_state->blocked &= ~newmask;
                std::snprintf(log_buf, sizeof(log_buf),
                              "[SIGPROCMASK] Unblocked signals: 0x%lx\n",
                              newmask);
                early_serial.write(log_buf);
                break;

            case SIG_SETMASK:
                // Set blocked set to new mask
                current->signal_state->blocked = newmask;
                std::snprintf(log_buf, sizeof(log_buf),
                              "[SIGPROCMASK] Set signal mask: 0x%lx\n",
                              newmask);
                early_serial.write(log_buf);
                break;

            default:
                return -EINVAL;
        }

        std::snprintf(log_buf, sizeof(log_buf),
                      "[SIGPROCMASK] New blocked mask: 0x%lx\n",
                      current->signal_state->blocked);
        early_serial.write(log_buf);
    }

    return 0;
}

// ============================================================================
// sys_sigreturn - Return from signal handler
// ============================================================================

/**
 * @brief sys_sigreturn - Return from signal handler
 *
 * Linux-specific: int sigreturn(void)
 *
 * This is called automatically by the signal trampoline after
 * the user's signal handler returns.
 *
 * Week 10 Phase 2: Simplified - just restore signal mask
 * Week 11: Full context restoration from signal frame
 *
 * @return Does not return (restores original context and resumes)
 */
extern "C" [[noreturn]] int64_t sys_sigreturn(uint64_t, uint64_t, uint64_t,
                                               uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) {
        early_serial.write("[SIGRETURN] FATAL: No current process\n");
        for(;;) { __asm__ volatile("hlt"); }
    }

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_sigreturn() for process %d\n",
                  current->pid);
    early_serial.write(log_buf);

    // Restore context from signal frame
    signal_return(current);

    // Week 10 Phase 2: Context restoration is simplified
    // The signal handler has already returned, so we just continue
    // Week 11: Restore full saved context from stack frame

    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGRETURN] Returning to user mode (RIP=0x%lx)\n",
                  current->context.rip);
    early_serial.write(log_buf);

    // Return to userspace
    schedule();
    __builtin_unreachable();
}

} // namespace xinim::kernel
