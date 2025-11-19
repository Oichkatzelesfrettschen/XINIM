/**
 * @file tty_signals.cpp
 * @brief TTY signal delivery for job control
 *
 * Implements signal delivery from terminal events:
 * - Ctrl+C → SIGINT to foreground process group
 * - Ctrl+Z → SIGTSTP to foreground process group
 * - Background read → SIGTTIN to background process
 * - Background write → SIGTTOU to background process
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "tty_signals.hpp"
#include "process_group.hpp"
#include "signal.hpp"
#include "pcb.hpp"
#include "scheduler.hpp"
#include "early/serial_16550.hpp"
#include <cstdio>
#include <cerrno>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// TTY Signal Delivery
// ============================================================================

/**
 * @brief Send SIGINT to foreground process group
 *
 * Called when user presses Ctrl+C in terminal.
 *
 * @param tty_session Session associated with this TTY
 * @return 0 on success, negative error on failure
 */
int tty_send_sigint(Session* tty_session) {
    if (!tty_session) {
        return -EINVAL;
    }

    ProcessGroup* fg = tty_session->foreground_group;
    if (!fg) {
        // No foreground process group - nothing to do
        early_serial.write("[TTY] Ctrl+C pressed, but no foreground process group\n");
        return 0;
    }

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[TTY] Ctrl+C: Sending SIGINT to foreground group %d\n",
                  fg->pgid);
    early_serial.write(buffer);

    return signal_process_group(fg->pgid, SIGINT);
}

/**
 * @brief Send SIGTSTP to foreground process group
 *
 * Called when user presses Ctrl+Z in terminal.
 * This stops the foreground job.
 *
 * @param tty_session Session associated with this TTY
 * @return 0 on success, negative error on failure
 */
int tty_send_sigtstp(Session* tty_session) {
    if (!tty_session) {
        return -EINVAL;
    }

    ProcessGroup* fg = tty_session->foreground_group;
    if (!fg) {
        // No foreground process group - nothing to do
        early_serial.write("[TTY] Ctrl+Z pressed, but no foreground process group\n");
        return 0;
    }

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[TTY] Ctrl+Z: Sending SIGTSTP to foreground group %d\n",
                  fg->pgid);
    early_serial.write(buffer);

    return signal_process_group(fg->pgid, SIGTSTP);
}

/**
 * @brief Send SIGQUIT to foreground process group
 *
 * Called when user presses Ctrl+\ in terminal.
 * This quits and dumps core (if core dumps enabled).
 *
 * @param tty_session Session associated with this TTY
 * @return 0 on success, negative error on failure
 */
int tty_send_sigquit(Session* tty_session) {
    if (!tty_session) {
        return -EINVAL;
    }

    ProcessGroup* fg = tty_session->foreground_group;
    if (!fg) {
        // No foreground process group - nothing to do
        early_serial.write("[TTY] Ctrl+\\ pressed, but no foreground process group\n");
        return 0;
    }

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[TTY] Ctrl+\\: Sending SIGQUIT to foreground group %d\n",
                  fg->pgid);
    early_serial.write(buffer);

    return signal_process_group(fg->pgid, SIGQUIT);
}

/**
 * @brief Check if process can read from TTY
 *
 * Background processes cannot read from TTY unless:
 * 1. They ignore SIGTTIN
 * 2. They block SIGTTIN
 * 3. They are orphaned (parent exited)
 *
 * If blocked, sends SIGTTIN and returns false.
 *
 * @param pcb Process attempting to read
 * @param tty_session Session associated with TTY
 * @return true if read allowed, false if blocked
 */
bool tty_check_read_access(ProcessControlBlock* pcb, Session* tty_session) {
    if (!pcb || !tty_session) {
        return false;
    }

    // Find process's process group
    ProcessGroup* pg = find_process_group(pcb->pgid);
    if (!pg) {
        return false;
    }

    // If this is the foreground group, allow read
    if (tty_session->foreground_group == pg) {
        return true;
    }

    // Background process trying to read
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[TTY] Background process %d (group %d) trying to read from TTY\n",
                  pcb->pid, pcb->pgid);
    early_serial.write(buffer);

    // Check if SIGTTIN is ignored or blocked
    if (pcb->signal_state) {
        SignalHandler* handler = &pcb->signal_state->handlers[SIGTTIN];

        // Ignored - allow read
        if (handler->handler == SIG_IGN) {
            early_serial.write("[TTY] SIGTTIN ignored, allowing read\n");
            return true;
        }

        // Blocked - allow read
        if (pcb->signal_state->blocked & (1ULL << SIGTTIN)) {
            early_serial.write("[TTY] SIGTTIN blocked, allowing read\n");
            return true;
        }
    }

    // Check if orphaned (parent exited)
    if (pcb->parent_pid == 0 || pcb->parent_pid == 1) {
        // Orphaned process - return EIO instead of blocking
        early_serial.write("[TTY] Orphaned process, returning EIO\n");
        return false;  // Caller should return -EIO
    }

    // Send SIGTTIN to process group
    std::snprintf(buffer, sizeof(buffer),
                  "[TTY] Sending SIGTTIN to background group %d\n",
                  pcb->pgid);
    early_serial.write(buffer);

    signal_process_group(pcb->pgid, SIGTTIN);

    return false;  // Block the read
}

/**
 * @brief Check if process can write to TTY
 *
 * Background processes cannot write to TTY unless:
 * 1. They ignore SIGTTOU
 * 2. They block SIGTTOU
 * 3. The TOSTOP flag is not set on the TTY
 * 4. They are orphaned (parent exited)
 *
 * If blocked, sends SIGTTOU and returns false.
 *
 * @param pcb Process attempting to write
 * @param tty_session Session associated with TTY
 * @param tostop_enabled Is TOSTOP flag set on TTY?
 * @return true if write allowed, false if blocked
 */
bool tty_check_write_access(ProcessControlBlock* pcb, Session* tty_session,
                             bool tostop_enabled) {
    if (!pcb || !tty_session) {
        return false;
    }

    // Find process's process group
    ProcessGroup* pg = find_process_group(pcb->pgid);
    if (!pg) {
        return false;
    }

    // If this is the foreground group, allow write
    if (tty_session->foreground_group == pg) {
        return true;
    }

    // If TOSTOP not enabled, allow background writes
    if (!tostop_enabled) {
        return true;
    }

    // Background process trying to write with TOSTOP enabled
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[TTY] Background process %d (group %d) trying to write to TTY\n",
                  pcb->pid, pcb->pgid);
    early_serial.write(buffer);

    // Check if SIGTTOU is ignored or blocked
    if (pcb->signal_state) {
        SignalHandler* handler = &pcb->signal_state->handlers[SIGTTOU];

        // Ignored - allow write
        if (handler->handler == SIG_IGN) {
            early_serial.write("[TTY] SIGTTOU ignored, allowing write\n");
            return true;
        }

        // Blocked - allow write
        if (pcb->signal_state->blocked & (1ULL << SIGTTOU)) {
            early_serial.write("[TTY] SIGTTOU blocked, allowing write\n");
            return true;
        }
    }

    // Check if orphaned (parent exited)
    if (pcb->parent_pid == 0 || pcb->parent_pid == 1) {
        // Orphaned process - return EIO instead of blocking
        early_serial.write("[TTY] Orphaned process, returning EIO\n");
        return false;  // Caller should return -EIO
    }

    // Send SIGTTOU to process group
    std::snprintf(buffer, sizeof(buffer),
                  "[TTY] Sending SIGTTOU to background group %d\n",
                  pcb->pgid);
    early_serial.write(buffer);

    signal_process_group(pcb->pgid, SIGTTOU);

    return false;  // Block the write
}

/**
 * @brief Handle keyboard input character
 *
 * Checks for special control characters and sends appropriate signals.
 * Returns true if character was handled (signal sent), false if normal input.
 *
 * @param ch Character from keyboard
 * @param tty_session Session associated with this TTY
 * @return true if character handled as control, false if normal input
 */
bool tty_handle_input_char(char ch, Session* tty_session) {
    if (!tty_session) {
        return false;
    }

    switch (ch) {
        case 0x03:  // Ctrl+C (ASCII ETX)
            tty_send_sigint(tty_session);
            return true;

        case 0x1A:  // Ctrl+Z (ASCII SUB)
            tty_send_sigtstp(tty_session);
            return true;

        case 0x1C:  // Ctrl+\ (ASCII FS)
            tty_send_sigquit(tty_session);
            return true;

        default:
            return false;  // Normal character
    }
}

/**
 * @brief Get session associated with current process's controlling TTY
 *
 * @param pcb Current process
 * @return Session pointer, or nullptr if no controlling TTY
 */
Session* tty_get_session(ProcessControlBlock* pcb) {
    if (!pcb) {
        return nullptr;
    }

    // Find process's session
    return find_session(pcb->sid);
}

} // namespace xinim::kernel
