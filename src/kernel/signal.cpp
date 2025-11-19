/**
 * @file signal.cpp
 * @brief Signal delivery mechanism implementation
 *
 * Implements POSIX signal delivery, masking, and handler invocation.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "signal.hpp"
#include "pcb.hpp"
#include "scheduler.hpp"
#include "../early/serial_16550.hpp"
#include <cstring>
#include <cstdio>
#include <cerrno>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// Signal Default Actions
// ============================================================================

/**
 * @brief Get default action for signal
 */
SignalDefaultAction get_default_action(int signum) {
    switch (signum) {
        // Terminate signals
        case SIGHUP:
        case SIGINT:
        case SIGALRM:
        case SIGTERM:
        case SIGUSR1:
        case SIGUSR2:
        case SIGPIPE:
        case SIGSTKFLT:
        case SIGXCPU:
        case SIGXFSZ:
        case SIGSYS:
        case SIGPWR:
            return SignalDefaultAction::TERM;

        // Terminate with core dump
        case SIGQUIT:
        case SIGILL:
        case SIGTRAP:
        case SIGABRT:
        case SIGBUS:
        case SIGFPE:
        case SIGSEGV:
            return SignalDefaultAction::CORE;

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

        // SIGKILL - special case (cannot be caught)
        case SIGKILL:
            return SignalDefaultAction::TERM;

        default:
            return SignalDefaultAction::TERM;
    }
}

// ============================================================================
// Signal State Management
// ============================================================================

/**
 * @brief Initialize signal state for new process
 */
void init_signal_state(ProcessControlBlock* pcb) {
    if (!pcb) return;

    // Allocate signal state if not already allocated
    if (!pcb->signal_state) {
        pcb->signal_state = new SignalState();
    }

    std::memset(pcb->signal_state, 0, sizeof(SignalState));

    // Set all handlers to SIG_DFL
    for (int i = 0; i < NSIG; i++) {
        pcb->signal_state->handlers[i].handler = SIG_DFL;
        pcb->signal_state->handlers[i].flags = 0;
        pcb->signal_state->handlers[i].mask = 0;
        pcb->signal_state->handlers[i].restorer = 0;
    }

    pcb->signal_state->pending = 0;
    pcb->signal_state->blocked = 0;
    pcb->signal_state->in_handler = false;
    pcb->signal_state->saved_mask = 0;

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Initialized signal state for process %d\n",
                  pcb->pid);
    early_serial.write(log_buf);
}

/**
 * @brief Reset signal handlers to default (for execve)
 */
void reset_signal_handlers(ProcessControlBlock* pcb) {
    if (!pcb || !pcb->signal_state) return;

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Resetting signal handlers for process %d\n",
                  pcb->pid);
    early_serial.write(log_buf);

    // Reset handlers but preserve mask and pending signals
    for (int i = 0; i < NSIG; i++) {
        if (pcb->signal_state->handlers[i].handler != SIG_DFL &&
            pcb->signal_state->handlers[i].handler != SIG_IGN) {
            // Reset custom handlers to default
            pcb->signal_state->handlers[i].handler = SIG_DFL;
            pcb->signal_state->handlers[i].flags = 0;
            pcb->signal_state->handlers[i].mask = 0;
        }
    }

    // Clear in_handler flag
    pcb->signal_state->in_handler = false;
}

// ============================================================================
// Signal Delivery
// ============================================================================

/**
 * @brief Send signal to process
 */
int send_signal(ProcessControlBlock* pcb, int signum) {
    if (!pcb) return -ESRCH;
    if (signum < 1 || signum >= NSIG) return -EINVAL;

    // Initialize signal state if not already initialized
    if (!pcb->signal_state) {
        init_signal_state(pcb);
    }

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Sending signal %d to process %d\n",
                  signum, pcb->pid);
    early_serial.write(log_buf);

    // SIGKILL and SIGSTOP cannot be caught or ignored - immediate effect
    if (signum == SIGKILL) {
        // Terminate process immediately
        pcb->state = ProcessState::ZOMBIE;
        pcb->exit_status = 128 + signum;  // Standard: 128 + signal number
        pcb->has_exited = true;

        std::snprintf(log_buf, sizeof(log_buf),
                      "[SIGNAL] SIGKILL: Process %d terminated immediately\n",
                      pcb->pid);
        early_serial.write(log_buf);
        return 0;
    }

    if (signum == SIGSTOP) {
        // Stop process immediately
        pcb->state = ProcessState::STOPPED;

        std::snprintf(log_buf, sizeof(log_buf),
                      "[SIGNAL] SIGSTOP: Process %d stopped\n",
                      pcb->pid);
        early_serial.write(log_buf);
        return 0;
    }

    // SIGCONT has special behavior - unstop if stopped
    if (signum == SIGCONT && pcb->state == ProcessState::STOPPED) {
        pcb->state = ProcessState::READY;
        early_serial.write("[SIGNAL] SIGCONT: Process resumed\n");
        // Still mark as pending for handler delivery
    }

    // Mark signal as pending
    pcb->signal_state->pending |= (1ULL << signum);

    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Signal %d pending (mask: 0x%lx)\n",
                  signum, pcb->signal_state->pending);
    early_serial.write(log_buf);

    // Wake process if it's blocked (for signal delivery)
    if (pcb->state == ProcessState::BLOCKED) {
        // Check if signal is unblocked
        if (!(pcb->signal_state->blocked & (1ULL << signum))) {
            pcb->state = ProcessState::READY;
            std::snprintf(log_buf, sizeof(log_buf),
                          "[SIGNAL] Woke blocked process %d for signal delivery\n",
                          pcb->pid);
            early_serial.write(log_buf);
        }
    }

    return 0;
}

/**
 * @brief Set up signal handler on user stack
 */
void setup_signal_frame(ProcessControlBlock* pcb, int signum) {
    if (!pcb || !pcb->signal_state) return;

    char log_buf[256];

    // Get handler
    SignalHandler* handler = &pcb->signal_state->handlers[signum];

    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Setting up signal frame: sig=%d handler=0x%lx\n",
                  signum, handler->handler);
    early_serial.write(log_buf);

    // Week 10 Phase 2: Simplified signal frame
    // We'll save minimal state and call the handler directly
    // Week 11: Full sigcontext with FPU state and proper trampoline

    // Save current signal mask
    pcb->signal_state->saved_mask = pcb->signal_state->blocked;

    // Block additional signals during handler (from sa_mask)
    pcb->signal_state->blocked |= handler->mask;

    // Block current signal (unless SA_NODEFER)
    if (!(handler->flags & SA_NODEFER)) {
        pcb->signal_state->blocked |= (1ULL << signum);
    }

    // Set up context to call signal handler
    // Signal handler signature: void handler(int signum)
    pcb->context.rdi = signum;  // First argument

    // Save return address on stack
    // Week 10 Phase 2: Simplified - we'll just update RIP
    // Week 11: Proper signal frame with saved context

    // Allocate minimal space on user stack (16 bytes for alignment)
    pcb->context.rsp -= 16;
    pcb->context.rsp &= ~15ULL;  // Ensure 16-byte alignment

    // Jump to handler
    pcb->context.rip = handler->handler;

    pcb->signal_state->in_handler = true;

    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Signal frame ready: RIP=0x%lx RSP=0x%lx blocked=0x%lx\n",
                  pcb->context.rip, pcb->context.rsp,
                  pcb->signal_state->blocked);
    early_serial.write(log_buf);
}

/**
 * @brief Deliver pending signals
 */
bool deliver_pending_signals(ProcessControlBlock* pcb) {
    if (!pcb || !pcb->signal_state) return false;

    // Don't deliver signals if already in handler
    // Week 10 Phase 2: Simple approach - one signal at a time
    // Week 11: Nested signal handlers
    if (pcb->signal_state->in_handler) {
        return false;
    }

    // Find highest-priority unblocked pending signal
    uint64_t deliverable = pcb->signal_state->pending & ~pcb->signal_state->blocked;

    if (deliverable == 0) {
        return false;  // No signals to deliver
    }

    // Find first set bit (lowest signal number = highest priority)
    int signum = __builtin_ffsll(deliverable);
    if (signum == 0) return false;

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Delivering signal %d to process %d\n",
                  signum, pcb->pid);
    early_serial.write(log_buf);

    // Clear pending bit
    pcb->signal_state->pending &= ~(1ULL << signum);

    // Get handler
    SignalHandler* handler = &pcb->signal_state->handlers[signum];

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
                pcb->has_exited = true;

                std::snprintf(log_buf, sizeof(log_buf),
                              "[SIGNAL] Process %d terminated by signal %d (default)\n",
                              pcb->pid, signum);
                early_serial.write(log_buf);
                return true;

            case SignalDefaultAction::CORE:
                // Terminate with core dump (Week 11: actual core dump)
                pcb->state = ProcessState::ZOMBIE;
                pcb->exit_status = 128 + signum;
                pcb->has_exited = true;

                std::snprintf(log_buf, sizeof(log_buf),
                              "[SIGNAL] Process %d terminated (core dump in Week 11)\n",
                              pcb->pid);
                early_serial.write(log_buf);
                return true;

            case SignalDefaultAction::IGNORE:
                // Ignore
                early_serial.write("[SIGNAL] Signal ignored (default action)\n");
                return false;

            case SignalDefaultAction::STOP:
                // Stop process
                pcb->state = ProcessState::STOPPED;
                early_serial.write("[SIGNAL] Process stopped (default)\n");
                return true;

            case SignalDefaultAction::CONT:
                // Continue process
                if (pcb->state == ProcessState::STOPPED) {
                    pcb->state = ProcessState::READY;
                    early_serial.write("[SIGNAL] Process continued (default)\n");
                }
                return false;
        }
    }

    // Custom handler - set up signal frame
    setup_signal_frame(pcb, signum);
    return true;
}

/**
 * @brief Return from signal handler
 */
void signal_return(ProcessControlBlock* pcb) {
    if (!pcb || !pcb->signal_state) return;

    char log_buf[128];
    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Returning from signal handler (process %d)\n",
                  pcb->pid);
    early_serial.write(log_buf);

    // Restore old signal mask
    pcb->signal_state->blocked = pcb->signal_state->saved_mask;

    std::snprintf(log_buf, sizeof(log_buf),
                  "[SIGNAL] Restored signal mask: 0x%lx\n",
                  pcb->signal_state->blocked);
    early_serial.write(log_buf);

    pcb->signal_state->in_handler = false;

    // Week 10 Phase 2: Context restoration is simplified
    // Week 11: Full context restoration from signal frame on stack
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Find process by PID
 *
 * Week 10 Phase 2: Simple linear search
 * Week 11: Hash table or better data structure
 */
ProcessControlBlock* find_process_by_pid(xinim::pid_t pid) {
    // This is a simplified implementation
    // Real implementation will search process table
    // For now, we'll need to access the scheduler's process list

    // TODO Week 10 Phase 2: Implement proper process lookup
    // This requires access to the global process table
    // For now, return nullptr - will be implemented with scheduler integration

    extern ProcessControlBlock* get_process_by_pid(xinim::pid_t pid);
    return get_process_by_pid(pid);
}

} // namespace xinim::kernel
