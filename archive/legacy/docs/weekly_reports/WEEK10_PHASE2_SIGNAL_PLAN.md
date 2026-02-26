# Week 10 Phase 2: Signal Framework - Detailed Plan

**Date**: November 18, 2025
**Status**: 🚀 READY TO START
**Phase**: Week 10 Phase 2 - Signal Handling
**Estimated Effort**: 10-12 hours
**Dependencies**: Week 10 Phase 1 complete (execve)

---

## Executive Summary

Week 10 Phase 2 implements a comprehensive POSIX signal framework, enabling processes to send, receive, and handle signals. This is critical for:
- Process communication (SIGCHLD, SIGTERM, SIGINT)
- Error handling (SIGSEGV, SIGFPE, SIGILL)
- IPC (SIGPIPE for broken pipes)
- Job control (SIGSTOP, SIGCONT, SIGTSTP)
- User-defined handlers (custom signal handling)

By the end of Phase 2, XINIM will support full POSIX signal semantics including delivery, masking, and user-space signal handlers.

---

## Technical Background

### POSIX Signals

**Signal**: Asynchronous notification sent to a process to notify it of an event.

**Standard Signals** (1-31):
```
SIGHUP     1   Hangup detected
SIGINT     2   Interrupt from keyboard (Ctrl+C)
SIGQUIT    3   Quit from keyboard
SIGILL     4   Illegal instruction
SIGTRAP    5   Trace/breakpoint trap
SIGABRT    6   Abort signal
SIGFPE     8   Floating-point exception
SIGKILL    9   Kill signal (cannot be caught)
SIGSEGV   11   Invalid memory reference
SIGPIPE   13   Broken pipe
SIGALRM   14   Timer signal
SIGTERM   15   Termination signal
SIGUSR1   10   User-defined signal 1
SIGUSR2   12   User-defined signal 2
SIGCHLD   17   Child stopped or terminated
SIGCONT   18   Continue if stopped
SIGSTOP   19   Stop process (cannot be caught)
SIGTSTP   20   Stop typed at terminal
```

### Signal Actions

Each signal has one of three dispositions:
1. **SIG_DFL** (default): Default action (terminate, ignore, stop, continue)
2. **SIG_IGN** (ignore): Ignore the signal
3. **Custom handler**: User-space function to handle signal

### Signal Delivery

**Delivery Process**:
1. Signal generated (kill, hardware exception, etc.)
2. Signal marked as pending in process's signal mask
3. On next kernel→user transition, check for pending signals
4. If signal not blocked, deliver it:
   - Save user context
   - Set up signal trampoline on user stack
   - Jump to signal handler
5. Signal handler executes in user mode
6. Handler calls sigreturn to restore context

### Signal Masking

Processes can **block** signals temporarily:
- Blocked signals remain pending but aren't delivered
- Unblocking a signal delivers it if pending
- SIGKILL and SIGSTOP cannot be blocked

---

## Implementation Plan

### Phase 2.1: Signal Data Structures (2-3 hours)

#### File: `src/kernel/signal.hpp` (250 LOC)

**Purpose**: Define signal structures, constants, and interface

```cpp
/**
 * @file signal.hpp
 * @brief POSIX signal framework
 *
 * Implements signal delivery, handling, and masking.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#pragma once
#include <cstdint>
#include "pcb.hpp"

namespace xinim::kernel {

// ============================================================================
// Signal Numbers (POSIX standard)
// ============================================================================

#define SIGHUP      1   // Hangup
#define SIGINT      2   // Interrupt
#define SIGQUIT     3   // Quit
#define SIGILL      4   // Illegal instruction
#define SIGTRAP     5   // Trace trap
#define SIGABRT     6   // Abort
#define SIGBUS      7   // Bus error
#define SIGFPE      8   // Floating point exception
#define SIGKILL     9   // Kill (cannot be caught or ignored)
#define SIGUSR1    10   // User defined signal 1
#define SIGSEGV    11   // Segmentation violation
#define SIGUSR2    12   // User defined signal 2
#define SIGPIPE    13   // Write to pipe with no readers
#define SIGALRM    14   // Alarm clock
#define SIGTERM    15   // Termination signal
#define SIGSTKFLT  16   // Stack fault
#define SIGCHLD    17   // Child status changed
#define SIGCONT    18   // Continue if stopped
#define SIGSTOP    19   // Stop (cannot be caught or ignored)
#define SIGTSTP    20   // Stop typed at terminal
#define SIGTTIN    21   // Background read from control terminal
#define SIGTTOU    22   // Background write to control terminal
#define SIGURG     23   // Urgent data on socket
#define SIGXCPU    24   // CPU time limit exceeded
#define SIGXFSZ    25   // File size limit exceeded
#define SIGVTALRM  26   // Virtual timer expired
#define SIGPROF    27   // Profiling timer expired
#define SIGWINCH   28   // Window size change
#define SIGIO      29   // I/O now possible
#define SIGPWR     30   // Power failure
#define SIGSYS     31   // Bad system call

#define NSIG       32   // Number of signals (including 0)

// Special signal handler values
#define SIG_DFL    ((uint64_t)0)   // Default handler
#define SIG_IGN    ((uint64_t)1)   // Ignore signal

// ============================================================================
// Signal Action Flags (for sigaction)
// ============================================================================

#define SA_NOCLDSTOP  0x00000001  // Don't receive SIGCHLD when child stops
#define SA_NOCLDWAIT  0x00000002  // Don't create zombie on child death
#define SA_SIGINFO    0x00000004  // Use sa_sigaction instead of sa_handler
#define SA_ONSTACK    0x08000000  // Use signal stack
#define SA_RESTART    0x10000000  // Restart syscalls interrupted by handler
#define SA_NODEFER    0x40000000  // Don't block signal while handler runs
#define SA_RESETHAND  0x80000000  // Reset to SIG_DFL after handler runs

// ============================================================================
// Signal Mask Operations (for sigprocmask)
// ============================================================================

#define SIG_BLOCK     0  // Add signals to mask
#define SIG_UNBLOCK   1  // Remove signals from mask
#define SIG_SETMASK   2  // Set mask to given value

// ============================================================================
// Signal Handler Structure
// ============================================================================

/**
 * @brief Signal handler entry
 *
 * Stores handler function and flags for each signal.
 */
struct SignalHandler {
    uint64_t handler;      // Handler function address (SIG_DFL, SIG_IGN, or user function)
    uint64_t flags;        // SA_* flags
    uint64_t mask;         // Signals blocked during handler execution
    uint64_t restorer;     // Signal restorer function (unused in Week 10)
};

/**
 * @brief Signal state for a process
 *
 * Stored in ProcessControlBlock.
 */
struct SignalState {
    SignalHandler handlers[NSIG];  // Signal handlers (indices 1-31)
    uint64_t pending;              // Pending signals bitmask (bit N = signal N)
    uint64_t blocked;              // Blocked signals bitmask
    bool in_handler;               // Currently executing signal handler
    uint64_t saved_mask;           // Saved signal mask (for handler execution)
};

// ============================================================================
// User-space structures (for syscalls)
// ============================================================================

/**
 * @brief sigaction structure (user-space)
 *
 * Matches Linux sigaction structure.
 */
struct sigaction_user {
    uint64_t sa_handler;    // Handler function or SIG_DFL/SIG_IGN
    uint64_t sa_flags;      // SA_* flags
    uint64_t sa_restorer;   // Restorer function (deprecated)
    uint64_t sa_mask;       // Signals blocked during handler
};

// ============================================================================
// Signal Framework Interface
// ============================================================================

/**
 * @brief Initialize signal state for new process
 *
 * Sets all handlers to SIG_DFL, clears pending/blocked masks.
 *
 * @param pcb Process control block
 */
void init_signal_state(ProcessControlBlock* pcb);

/**
 * @brief Reset signal handlers to default (for execve)
 *
 * Called by execve to reset custom handlers to SIG_DFL.
 * Preserves signal mask and pending signals.
 *
 * @param pcb Process control block
 */
void reset_signal_handlers(ProcessControlBlock* pcb);

/**
 * @brief Send signal to process
 *
 * Marks signal as pending. If process is blocked on wait or I/O,
 * wakes it up to deliver signal.
 *
 * @param pcb Target process
 * @param signum Signal number (1-31)
 * @return 0 on success, negative error on failure
 */
int send_signal(ProcessControlBlock* pcb, int signum);

/**
 * @brief Check and deliver pending signals
 *
 * Called before returning from kernel to user mode.
 * Delivers highest-priority unblocked pending signal.
 *
 * @param pcb Current process
 * @return true if signal was delivered (context modified)
 */
bool deliver_pending_signals(ProcessControlBlock* pcb);

/**
 * @brief Set up signal handler on user stack
 *
 * Modifies user context to call signal handler.
 * Saves original context for sigreturn.
 *
 * @param pcb Process control block
 * @param signum Signal number
 */
void setup_signal_frame(ProcessControlBlock* pcb, int signum);

/**
 * @brief Return from signal handler
 *
 * Restores original user context saved by setup_signal_frame.
 *
 * @param pcb Process control block
 */
void signal_return(ProcessControlBlock* pcb);

/**
 * @brief Get default action for signal
 *
 * @param signum Signal number
 * @return Default action (TERM, IGNORE, STOP, CONT)
 */
enum class SignalDefaultAction {
    TERM,    // Terminate process
    IGNORE,  // Ignore signal
    STOP,    // Stop process
    CONT,    // Continue process
    CORE     // Terminate and dump core
};

SignalDefaultAction get_default_action(int signum);

} // namespace xinim::kernel
```

---

### Phase 2.2: Signal Delivery Mechanism (3-4 hours)

#### File: `src/kernel/signal.cpp` (450 LOC)

**Purpose**: Implement signal delivery, masking, and handler invocation

```cpp
/**
 * @file signal.cpp
 * @brief Signal delivery mechanism implementation
 */

#include "signal.hpp"
#include "scheduler.hpp"
#include "../early/serial_16550.hpp"
#include <cstring>
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

/**
 * @brief Get default action for signal
 */
SignalDefaultAction get_default_action(int signum) {
    switch (signum) {
        // Terminate signals
        case SIGHUP:
        case SIGINT:
        case SIGQUIT:
        case SIGILL:
        case SIGTRAP:
        case SIGABRT:
        case SIGBUS:
        case SIGFPE:
        case SIGKILL:
        case SIGSEGV:
        case SIGPIPE:
        case SIGALRM:
        case SIGTERM:
        case SIGUSR1:
        case SIGUSR2:
        case SIGSTKFLT:
        case SIGXCPU:
        case SIGXFSZ:
        case SIGSYS:
            return SignalDefaultAction::TERM;

        // Ignore signals
        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
            return SignalDefaultAction::IGNORE;

        // Stop signals
        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            return SignalDefaultAction::STOP;

        // Continue signal
        case SIGCONT:
            return SignalDefaultAction::CONT;

        // Core dump signals (Week 11)
        case SIGQUIT:
        case SIGILL:
        case SIGABRT:
        case SIGFPE:
        case SIGSEGV:
            return SignalDefaultAction::CORE;

        default:
            return SignalDefaultAction::TERM;
    }
}

/**
 * @brief Initialize signal state for new process
 */
void init_signal_state(ProcessControlBlock* pcb) {
    if (!pcb) return;

    std::memset(&pcb->signal_state, 0, sizeof(SignalState));

    // Set all handlers to SIG_DFL
    for (int i = 0; i < NSIG; i++) {
        pcb->signal_state.handlers[i].handler = SIG_DFL;
        pcb->signal_state.handlers[i].flags = 0;
        pcb->signal_state.handlers[i].mask = 0;
    }

    pcb->signal_state.pending = 0;
    pcb->signal_state.blocked = 0;
    pcb->signal_state.in_handler = false;
}

/**
 * @brief Reset signal handlers to default (for execve)
 */
void reset_signal_handlers(ProcessControlBlock* pcb) {
    if (!pcb) return;

    // Reset handlers but preserve mask and pending signals
    for (int i = 0; i < NSIG; i++) {
        if (pcb->signal_state.handlers[i].handler != SIG_DFL &&
            pcb->signal_state.handlers[i].handler != SIG_IGN) {
            // Reset custom handlers to default
            pcb->signal_state.handlers[i].handler = SIG_DFL;
            pcb->signal_state.handlers[i].flags = 0;
            pcb->signal_state.handlers[i].mask = 0;
        }
    }

    early_serial.write("[SIGNAL] Signal handlers reset to default\n");
}

/**
 * @brief Send signal to process
 */
int send_signal(ProcessControlBlock* pcb, int signum) {
    if (!pcb) return -ESRCH;
    if (signum < 1 || signum >= NSIG) return -EINVAL;

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Sending signal %d to process %d\n",
                  signum, pcb->pid);
    early_serial.write(log_buf);

    // SIGKILL and SIGSTOP cannot be caught or ignored
    if (signum == SIGKILL || signum == SIGSTOP) {
        // These signals take immediate effect
        if (signum == SIGKILL) {
            // Terminate process immediately
            pcb->state = ProcessState::ZOMBIE;
            pcb->exit_status = 128 + signum;  // Standard: 128 + signal number
            early_serial.write("[SIGNAL] SIGKILL: Process terminated\n");
        } else if (signum == SIGSTOP) {
            // Stop process
            pcb->state = ProcessState::STOPPED;
            early_serial.write("[SIGNAL] SIGSTOP: Process stopped\n");
        }
        return 0;
    }

    // Mark signal as pending
    pcb->signal_state.pending |= (1ULL << signum);

    // Wake process if it's blocked (for signal delivery)
    if (pcb->state == ProcessState::BLOCKED) {
        pcb->state = ProcessState::READY;
        std::snprintf(log_buf, sizeof(log_buf),
                      "[SIGNAL] Woke blocked process %d for signal delivery\n",
                      pcb->pid);
        early_serial.write(log_buf);
    }

    return 0;
}

/**
 * @brief Set up signal handler on user stack
 */
void setup_signal_frame(ProcessControlBlock* pcb, int signum) {
    char log_buf[256];

    // Get handler
    SignalHandler* handler = &pcb->signal_state.handlers[signum];

    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Setting up signal frame for signal %d, handler=0x%lx\n",
                  signum, handler->handler);
    early_serial.write(log_buf);

    // Save current user context
    // We'll store it on the user stack

    // Week 10 Phase 2: Simplified signal frame
    // Week 11: Full sigcontext with FPU state

    // Allocate space on user stack for signal frame
    uint64_t new_rsp = pcb->context.rsp - 256;  // 256 bytes for frame

    // Week 10 Phase 2: We'll just save essential state
    // Layout:
    // [new_rsp + 0]: old RIP
    // [new_rsp + 8]: old RSP
    // [new_rsp + 16]: old RFLAGS
    // [new_rsp + 24]: signal number
    // [new_rsp + 32]: old signal mask

    // TODO Week 10 Phase 2: Write signal frame to user stack
    // For now, we'll update context directly

    // Save current mask
    pcb->signal_state.saved_mask = pcb->signal_state.blocked;

    // Block additional signals during handler (if specified)
    if (handler->mask) {
        pcb->signal_state.blocked |= handler->mask;
    }

    // Block current signal (unless SA_NODEFER)
    if (!(handler->flags & SA_NODEFER)) {
        pcb->signal_state.blocked |= (1ULL << signum);
    }

    // Set up context to call signal handler
    // Arguments: handler(int signum)
    pcb->context.rdi = signum;  // First argument

    // Return address: sigreturn syscall
    // Week 10 Phase 2: We'll use a simple approach
    // Week 11: Proper signal trampoline

    // Jump to handler
    pcb->context.rip = handler->handler;
    pcb->context.rsp = new_rsp;

    pcb->signal_state.in_handler = true;

    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Signal frame ready: RIP=0x%lx RSP=0x%lx\n",
                  pcb->context.rip, pcb->context.rsp);
    early_serial.write(log_buf);
}

/**
 * @brief Deliver pending signals
 */
bool deliver_pending_signals(ProcessControlBlock* pcb) {
    if (!pcb) return false;

    // Don't deliver signals if already in handler
    if (pcb->signal_state.in_handler) {
        return false;
    }

    // Find highest-priority unblocked pending signal
    uint64_t deliverable = pcb->signal_state.pending & ~pcb->signal_state.blocked;

    if (deliverable == 0) {
        return false;  // No signals to deliver
    }

    // Find first set bit (lowest signal number has highest priority)
    int signum = __builtin_ffsll(deliverable);
    if (signum == 0) return false;

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Delivering signal %d to process %d\n",
                  signum, pcb->pid);
    early_serial.write(log_buf);

    // Clear pending bit
    pcb->signal_state.pending &= ~(1ULL << signum);

    // Get handler
    SignalHandler* handler = &pcb->signal_state.handlers[signum];

    if (handler->handler == SIG_IGN) {
        // Ignore signal
        early_serial.write("[SIGNAL] Signal ignored (SIG_IGN)\n");
        return false;
    }

    if (handler->handler == SIG_DFL) {
        // Default action
        SignalDefaultAction action = get_default_action(signum);

        switch (action) {
            case SignalDefaultAction::TERM:
                // Terminate process
                pcb->state = ProcessState::ZOMBIE;
                pcb->exit_status = 128 + signum;
                std::snprintf(log_buf, sizeof(log_buf),
                              "[SIGNAL] Process %d terminated by signal %d\n",
                              pcb->pid, signum);
                early_serial.write(log_buf);
                return true;

            case SignalDefaultAction::IGNORE:
                // Ignore
                early_serial.write("[SIGNAL] Signal ignored (default)\n");
                return false;

            case SignalDefaultAction::STOP:
                // Stop process
                pcb->state = ProcessState::STOPPED;
                early_serial.write("[SIGNAL] Process stopped\n");
                return true;

            case SignalDefaultAction::CONT:
                // Continue process
                if (pcb->state == ProcessState::STOPPED) {
                    pcb->state = ProcessState::READY;
                    early_serial.write("[SIGNAL] Process continued\n");
                }
                return false;

            case SignalDefaultAction::CORE:
                // Terminate with core dump (Week 11)
                pcb->state = ProcessState::ZOMBIE;
                pcb->exit_status = 128 + signum;
                early_serial.write("[SIGNAL] Process terminated (core dump in Week 11)\n");
                return true;
        }
    }

    // Custom handler
    setup_signal_frame(pcb, signum);
    return true;
}

/**
 * @brief Return from signal handler
 */
void signal_return(ProcessControlBlock* pcb) {
    if (!pcb) return;

    early_serial.write("[SIGNAL] Returning from signal handler\n");

    // TODO Week 10 Phase 2: Restore saved context from stack
    // For now, just restore signal mask

    // Restore old signal mask
    pcb->signal_state.blocked = pcb->signal_state.saved_mask;

    pcb->signal_state.in_handler = false;

    // Context will be restored from signal frame on stack
    // Week 10 Phase 2: Simplified - just continue execution
}

} // namespace xinim::kernel
```

---

### Phase 2.3: Signal Syscalls (3-4 hours)

#### File: `src/kernel/syscalls/signal.cpp` (400 LOC)

**Purpose**: Implement sigaction, kill, sigprocmask, sigreturn syscalls

```cpp
/**
 * @file signal.cpp
 * @brief Signal-related syscalls
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
        // TODO: Check if process exists
        return 0;
    }

    // Find target process
    ProcessControlBlock* target = find_process_by_pid((xinim::pid_t)pid);
    if (!target) {
        return -ESRCH;  // No such process
    }

    // TODO Week 10 Phase 2: Check permissions
    // For now, allow all signals

    // Send signal
    return send_signal(target, (int)sig);
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
        return -EINVAL;
    }

    SignalHandler* handler = &current->signal_state.handlers[signum];

    // Return old action if requested
    if (oldact_addr != 0) {
        sigaction_user oldact;
        oldact.sa_handler = handler->handler;
        oldact.sa_flags = handler->flags;
        oldact.sa_mask = handler->mask;
        oldact.sa_restorer = 0;

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

        std::snprintf(log_buf, sizeof(log_buf),
                      "[SIGACTION] Signal %lu: handler=0x%lx flags=0x%lx\n",
                      signum, handler->handler, handler->flags);
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

    char log_buf[256];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SYSCALL] sys_sigprocmask(%lu, 0x%lx, 0x%lx)\n",
                  how, set_addr, oldset_addr);
    early_serial.write(log_buf);

    // Return old mask if requested
    if (oldset_addr != 0) {
        uint64_t oldmask = current->signal_state.blocked;
        int ret = copy_to_user(oldset_addr, &oldmask, sizeof(uint64_t));
        if (ret < 0) return ret;
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
                current->signal_state.blocked |= newmask;
                early_serial.write("[SIGPROCMASK] Blocked additional signals\n");
                break;

            case SIG_UNBLOCK:
                current->signal_state.blocked &= ~newmask;
                early_serial.write("[SIGPROCMASK] Unblocked signals\n");
                break;

            case SIG_SETMASK:
                current->signal_state.blocked = newmask;
                early_serial.write("[SIGPROCMASK] Set signal mask\n");
                break;

            default:
                return -EINVAL;
        }

        std::snprintf(log_buf, sizeof(log_buf),
                      "[SIGPROCMASK] New mask: 0x%lx\n",
                      current->signal_state.blocked);
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
 * Linux: int sigreturn(void)
 *
 * This is called automatically by the signal trampoline.
 *
 * @return Does not return (restores original context)
 */
extern "C" [[noreturn]] int64_t sys_sigreturn(uint64_t, uint64_t, uint64_t,
                                               uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) {
        early_serial.write("[SIGRETURN] FATAL: No current process\n");
        for(;;) { __asm__ volatile("hlt"); }
    }

    early_serial.write("[SYSCALL] sys_sigreturn()\n");

    // Restore context from signal frame
    signal_return(current);

    // TODO Week 10 Phase 2: Restore full context from stack
    // For now, just continue with current context

    // Return to user mode
    schedule();
    __builtin_unreachable();
}

} // namespace xinim::kernel
```

---

## Integration Points

### 1. Update PCB Structure

Add signal state to `src/kernel/pcb.hpp`:
```cpp
#include "signal.hpp"

struct ProcessControlBlock {
    // ... existing fields

    // Week 10 Phase 2: Signal state
    SignalState signal_state;
};
```

### 2. Update execve

In `src/kernel/syscalls/exec.cpp`, call `reset_signal_handlers()`:
```cpp
// Reset signal handlers to default
reset_signal_handlers(current);
```

### 3. Update sys_exit

In `src/kernel/syscalls/basic.cpp`, send SIGCHLD to parent:
```cpp
// Send SIGCHLD to parent
if (parent) {
    send_signal(parent, SIGCHLD);
}
```

### 4. Update Pipe Write

In `src/kernel/pipe.cpp`, send SIGPIPE on broken pipe:
```cpp
if (!read_end_open) {
    // Send SIGPIPE to writing process
    send_signal(get_current_process(), SIGPIPE);
    return -EPIPE;
}
```

### 5. Update Scheduler

In `src/kernel/scheduler.cpp`, check for pending signals before returning to user mode:
```cpp
// Before returning to user mode
if (current->context.cs & 3) {  // Ring 3 (user mode)
    deliver_pending_signals(current);
}
```

---

## Testing Strategy

### Unit Tests
1. Signal sending and pending
2. Signal masking (block/unblock)
3. Default actions (TERM, IGNORE, STOP)
4. Custom handlers

### Integration Tests
1. Parent receives SIGCHLD on child exit
2. Process receives SIGPIPE on broken pipe write
3. SIGKILL terminates process
4. SIGSTOP stops process, SIGCONT continues
5. Custom handler modifies behavior
6. Signal masking blocks delivery

---

## Success Criteria

- ✅ Signals can be sent between processes
- ✅ SIGCHLD delivered on child exit
- ✅ SIGPIPE delivered on broken pipe
- ✅ Default actions work (terminate, ignore, stop)
- ✅ Custom signal handlers can be installed
- ✅ Signal masking works (block/unblock)
- ✅ SIGKILL and SIGSTOP cannot be caught
- ✅ execve resets signal handlers

---

## Files to Create

| File | LOC | Purpose |
|------|-----|---------|
| `src/kernel/signal.hpp` | 250 | Signal structures and interface |
| `src/kernel/signal.cpp` | 450 | Signal delivery mechanism |
| `src/kernel/syscalls/signal.cpp` | 400 | Signal syscalls |
| `docs/WEEK10_PHASE2_COMPLETE.md` | 800 | Completion report |

**Total Week 10 Phase 2**: ~1,900 LOC

---

**Week 10 Phase 2 Plan Complete**
**Ready to implement signal framework**
