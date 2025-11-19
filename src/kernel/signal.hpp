/**
 * @file signal.hpp
 * @brief POSIX signal framework
 *
 * Implements signal delivery, handling, and masking according to
 * POSIX.1-2008 specification.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#pragma once
#include <cstdint>

namespace xinim {
// Forward declaration
typedef int32_t pid_t;
}

namespace xinim::kernel {

// Forward declaration
struct ProcessControlBlock;

// ============================================================================
// Signal Numbers (POSIX standard)
// ============================================================================

#define SIGHUP      1   ///< Hangup
#define SIGINT      2   ///< Interrupt (Ctrl+C)
#define SIGQUIT     3   ///< Quit (Ctrl+\)
#define SIGILL      4   ///< Illegal instruction
#define SIGTRAP     5   ///< Trace trap
#define SIGABRT     6   ///< Abort
#define SIGBUS      7   ///< Bus error
#define SIGFPE      8   ///< Floating point exception
#define SIGKILL     9   ///< Kill (cannot be caught or ignored)
#define SIGUSR1    10   ///< User defined signal 1
#define SIGSEGV    11   ///< Segmentation violation
#define SIGUSR2    12   ///< User defined signal 2
#define SIGPIPE    13   ///< Write to pipe with no readers
#define SIGALRM    14   ///< Alarm clock
#define SIGTERM    15   ///< Termination signal
#define SIGSTKFLT  16   ///< Stack fault
#define SIGCHLD    17   ///< Child status changed
#define SIGCONT    18   ///< Continue if stopped
#define SIGSTOP    19   ///< Stop (cannot be caught or ignored)
#define SIGTSTP    20   ///< Stop typed at terminal (Ctrl+Z)
#define SIGTTIN    21   ///< Background read from control terminal
#define SIGTTOU    22   ///< Background write to control terminal
#define SIGURG     23   ///< Urgent data on socket
#define SIGXCPU    24   ///< CPU time limit exceeded
#define SIGXFSZ    25   ///< File size limit exceeded
#define SIGVTALRM  26   ///< Virtual timer expired
#define SIGPROF    27   ///< Profiling timer expired
#define SIGWINCH   28   ///< Window size change
#define SIGIO      29   ///< I/O now possible
#define SIGPWR     30   ///< Power failure
#define SIGSYS     31   ///< Bad system call

#define NSIG       32   ///< Number of signals (including signal 0)

// Special signal handler values
#define SIG_DFL    ((uint64_t)0)   ///< Default handler
#define SIG_IGN    ((uint64_t)1)   ///< Ignore signal

// ============================================================================
// Signal Action Flags (for sigaction)
// ============================================================================

#define SA_NOCLDSTOP  0x00000001  ///< Don't receive SIGCHLD when child stops
#define SA_NOCLDWAIT  0x00000002  ///< Don't create zombie on child death
#define SA_SIGINFO    0x00000004  ///< Use sa_sigaction instead of sa_handler
#define SA_ONSTACK    0x08000000  ///< Use signal stack
#define SA_RESTART    0x10000000  ///< Restart syscalls interrupted by handler
#define SA_NODEFER    0x40000000  ///< Don't block signal while handler runs
#define SA_RESETHAND  0x80000000  ///< Reset to SIG_DFL after handler runs

// ============================================================================
// Signal Mask Operations (for sigprocmask)
// ============================================================================

#define SIG_BLOCK     0  ///< Add signals to mask
#define SIG_UNBLOCK   1  ///< Remove signals from mask
#define SIG_SETMASK   2  ///< Set mask to given value

// ============================================================================
// Signal Handler Structure
// ============================================================================

/**
 * @brief Signal handler entry
 *
 * Stores handler function and flags for each signal.
 */
struct SignalHandler {
    uint64_t handler;      ///< Handler function address (SIG_DFL, SIG_IGN, or user function)
    uint64_t flags;        ///< SA_* flags
    uint64_t mask;         ///< Signals blocked during handler execution
    uint64_t restorer;     ///< Signal restorer function (legacy, unused)
};

/**
 * @brief Signal state for a process
 *
 * Stored in ProcessControlBlock.
 */
struct SignalState {
    SignalHandler handlers[NSIG];  ///< Signal handlers (indices 0-31, only 1-31 used)
    uint64_t pending;              ///< Pending signals bitmask (bit N = signal N)
    uint64_t blocked;              ///< Blocked signals bitmask
    bool in_handler;               ///< Currently executing signal handler
    uint64_t saved_mask;           ///< Saved signal mask (for handler execution)
};

// ============================================================================
// User-space structures (for syscalls)
// ============================================================================

/**
 * @brief sigaction structure (user-space)
 *
 * Matches Linux sigaction structure layout.
 */
struct sigaction_user {
    uint64_t sa_handler;    ///< Handler function or SIG_DFL/SIG_IGN
    uint64_t sa_flags;      ///< SA_* flags
    uint64_t sa_restorer;   ///< Restorer function (deprecated)
    uint64_t sa_mask;       ///< Signals blocked during handler
} __attribute__((packed));

// ============================================================================
// Signal Framework Interface
// ============================================================================

/**
 * @brief Default action for signals
 */
enum class SignalDefaultAction {
    TERM,    ///< Terminate process
    IGNORE,  ///< Ignore signal
    STOP,    ///< Stop process
    CONT,    ///< Continue process
    CORE     ///< Terminate and dump core
};

/**
 * @brief Get default action for signal
 *
 * @param signum Signal number (1-31)
 * @return Default action for the signal
 */
SignalDefaultAction get_default_action(int signum);

/**
 * @brief Initialize signal state for new process
 *
 * Sets all handlers to SIG_DFL, clears pending/blocked masks.
 * Called during process creation (fork, init).
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
 * Special cases:
 * - SIGKILL: Immediate termination (cannot be caught)
 * - SIGSTOP: Immediate stop (cannot be caught)
 *
 * @param pcb Target process
 * @param signum Signal number (1-31)
 * @return 0 on success, negative error on failure
 *         -EINVAL: Invalid signal number
 */
int send_signal(ProcessControlBlock* pcb, int signum);

/**
 * @brief Check and deliver pending signals
 *
 * Called before returning from kernel to user mode.
 * Delivers highest-priority unblocked pending signal.
 *
 * Priority order: lowest signal number first (SIGHUP=1 highest priority).
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
 * Week 10 Phase 2: Simplified signal frame
 * Week 11: Full sigcontext with FPU state
 *
 * @param pcb Process control block
 * @param signum Signal number
 */
void setup_signal_frame(ProcessControlBlock* pcb, int signum);

/**
 * @brief Return from signal handler
 *
 * Restores original user context saved by setup_signal_frame.
 * Called automatically by signal trampoline or sys_sigreturn.
 *
 * @param pcb Process control block
 */
void signal_return(ProcessControlBlock* pcb);

/**
 * @brief Find process by PID
 *
 * Helper function for sys_kill.
 *
 * @param pid Process ID
 * @return Process control block or nullptr if not found
 */
ProcessControlBlock* find_process_by_pid(xinim::pid_t pid);

} // namespace xinim::kernel
