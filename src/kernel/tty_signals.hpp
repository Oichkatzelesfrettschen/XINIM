/**
 * @file tty_signals.hpp
 * @brief TTY signal delivery interface
 *
 * Functions for delivering signals from terminal events.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_TTY_SIGNALS_HPP
#define XINIM_KERNEL_TTY_SIGNALS_HPP

namespace xinim::kernel {

// Forward declarations
struct ProcessControlBlock;
struct Session;

// ============================================================================
// TTY Signal Delivery Functions
// ============================================================================

/**
 * @brief Send SIGINT to foreground process group (Ctrl+C)
 *
 * @param tty_session Session associated with TTY
 * @return 0 on success, negative error on failure
 */
int tty_send_sigint(Session* tty_session);

/**
 * @brief Send SIGTSTP to foreground process group (Ctrl+Z)
 *
 * @param tty_session Session associated with TTY
 * @return 0 on success, negative error on failure
 */
int tty_send_sigtstp(Session* tty_session);

/**
 * @brief Send SIGQUIT to foreground process group (Ctrl+\)
 *
 * @param tty_session Session associated with TTY
 * @return 0 on success, negative error on failure
 */
int tty_send_sigquit(Session* tty_session);

/**
 * @brief Check if process can read from TTY
 *
 * Background processes are blocked (sent SIGTTIN) unless signal is ignored/blocked.
 *
 * @param pcb Process attempting to read
 * @param tty_session Session associated with TTY
 * @return true if read allowed, false if blocked
 */
bool tty_check_read_access(ProcessControlBlock* pcb, Session* tty_session);

/**
 * @brief Check if process can write to TTY
 *
 * Background processes may be blocked (sent SIGTTOU) if TOSTOP flag is set.
 *
 * @param pcb Process attempting to write
 * @param tty_session Session associated with TTY
 * @param tostop_enabled Is TOSTOP flag set on TTY?
 * @return true if write allowed, false if blocked
 */
bool tty_check_write_access(ProcessControlBlock* pcb, Session* tty_session,
                             bool tostop_enabled);

/**
 * @brief Handle keyboard input character
 *
 * Checks for control characters (Ctrl+C, Ctrl+Z, etc.) and sends signals.
 *
 * @param ch Character from keyboard
 * @param tty_session Session associated with TTY
 * @return true if character handled as control, false if normal input
 */
bool tty_handle_input_char(char ch, Session* tty_session);

/**
 * @brief Get session for current process's controlling TTY
 *
 * @param pcb Current process
 * @return Session pointer, or nullptr if no controlling TTY
 */
Session* tty_get_session(ProcessControlBlock* pcb);

} // namespace xinim::kernel

#endif // XINIM_KERNEL_TTY_SIGNALS_HPP
