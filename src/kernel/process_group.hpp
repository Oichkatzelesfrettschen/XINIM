/**
 * @file process_group.hpp
 * @brief POSIX process groups and sessions
 *
 * Implements POSIX process groups and sessions for job control.
 * Required for shell implementation and TTY signal delivery.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_PROCESS_GROUP_HPP
#define XINIM_KERNEL_PROCESS_GROUP_HPP

#include <cstdint>
#include "../../include/xinim/types.h"

namespace xinim::kernel {

// Forward declaration
struct ProcessControlBlock;

// ============================================================================
// Process Group
// ============================================================================

/**
 * @brief Process group structure
 *
 * A process group is a collection of processes that share the same PGID.
 * Used for job control and signal delivery.
 *
 * POSIX semantics:
 * - Each process belongs to exactly one process group
 * - Process groups are identified by PGID (same as leader's PID)
 * - Process group leader has PID == PGID
 * - Signals can be sent to entire process group
 */
struct ProcessGroup {
    xinim::pid_t pgid;              ///< Process group ID (leader's PID)
    xinim::pid_t session_id;        ///< Session this group belongs to
    ProcessControlBlock* members;   ///< Linked list of member processes
    ProcessGroup* next;             ///< Next group in global list
    ProcessGroup* prev;             ///< Previous group in global list
    int member_count;               ///< Number of processes in group
    bool is_foreground;             ///< Is this the foreground process group?
};

// ============================================================================
// Session
// ============================================================================

/**
 * @brief Session structure
 *
 * A session is a collection of process groups. Each session can have
 * a controlling terminal.
 *
 * POSIX semantics:
 * - Each process belongs to exactly one session
 * - Sessions are identified by SID (same as leader's PID)
 * - Session leader has PID == SID
 * - setsid() creates new session and process group
 * - Only one foreground process group per session
 */
struct Session {
    xinim::pid_t sid;               ///< Session ID (leader's PID)
    ProcessGroup* groups;           ///< Linked list of process groups
    ProcessGroup* foreground_group; ///< Current foreground process group
    void* controlling_tty;          ///< Controlling terminal (TTY device)
    Session* next;                  ///< Next session in global list
    Session* prev;                  ///< Previous session in global list
    int group_count;                ///< Number of process groups in session
};

// ============================================================================
// Process Group Management Functions
// ============================================================================

/**
 * @brief Initialize process group subsystem
 */
void init_process_groups();

/**
 * @brief Create a new process group
 *
 * @param pgid Process group ID (usually leader's PID)
 * @param sid Session ID
 * @return Pointer to new process group, or nullptr on failure
 */
ProcessGroup* create_process_group(xinim::pid_t pgid, xinim::pid_t sid);

/**
 * @brief Find process group by PGID
 *
 * @param pgid Process group ID
 * @return Pointer to process group, or nullptr if not found
 */
ProcessGroup* find_process_group(xinim::pid_t pgid);

/**
 * @brief Add process to process group
 *
 * @param pcb Process to add
 * @param pg Process group
 */
void add_to_process_group(ProcessControlBlock* pcb, ProcessGroup* pg);

/**
 * @brief Remove process from its process group
 *
 * @param pcb Process to remove
 */
void remove_from_process_group(ProcessControlBlock* pcb);

/**
 * @brief Delete process group (when last member exits)
 *
 * @param pg Process group to delete
 */
void delete_process_group(ProcessGroup* pg);

/**
 * @brief Send signal to entire process group
 *
 * @param pgid Process group ID
 * @param sig Signal number
 * @return 0 on success, negative error code on failure
 */
int signal_process_group(xinim::pid_t pgid, int sig);

// ============================================================================
// Session Management Functions
// ============================================================================

/**
 * @brief Create a new session
 *
 * @param sid Session ID (usually leader's PID)
 * @return Pointer to new session, or nullptr on failure
 */
Session* create_session(xinim::pid_t sid);

/**
 * @brief Find session by SID
 *
 * @param sid Session ID
 * @return Pointer to session, or nullptr if not found
 */
Session* find_session(xinim::pid_t sid);

/**
 * @brief Delete session (when last process group exits)
 *
 * @param session Session to delete
 */
void delete_session(Session* session);

/**
 * @brief Set foreground process group for session
 *
 * @param session Session
 * @param pg Process group to make foreground (or nullptr for no foreground)
 * @return 0 on success, negative error code on failure
 */
int set_foreground_process_group(Session* session, ProcessGroup* pg);

/**
 * @brief Get foreground process group for session
 *
 * @param session Session
 * @return Foreground process group, or nullptr if none
 */
ProcessGroup* get_foreground_process_group(Session* session);

// ============================================================================
// Syscall Implementations (forward declarations)
// ============================================================================

/**
 * @brief sys_setpgid - Set process group ID
 *
 * POSIX: int setpgid(pid_t pid, pid_t pgid)
 *
 * Sets the PGID of process 'pid' to 'pgid'.
 * Special cases:
 * - pid == 0: use calling process
 * - pgid == 0: use pid as pgid (make process a group leader)
 *
 * @param pid Process ID (or 0 for current)
 * @param pgid Process group ID (or 0 for pid)
 * @return 0 on success, negative error code on failure
 */
int64_t sys_setpgid(uint64_t pid, uint64_t pgid,
                    uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief sys_getpgid - Get process group ID
 *
 * POSIX: pid_t getpgid(pid_t pid)
 *
 * @param pid Process ID (or 0 for current)
 * @return PGID on success, negative error code on failure
 */
int64_t sys_getpgid(uint64_t pid,
                    uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief sys_getpgrp - Get process group of calling process
 *
 * POSIX: pid_t getpgrp(void)
 *
 * @return PGID of calling process
 */
int64_t sys_getpgrp(uint64_t, uint64_t, uint64_t,
                    uint64_t, uint64_t, uint64_t);

/**
 * @brief sys_setsid - Create new session
 *
 * POSIX: pid_t setsid(void)
 *
 * Creates a new session with calling process as leader.
 * Process becomes leader of new session and new process group.
 *
 * @return SID on success, negative error code on failure
 */
int64_t sys_setsid(uint64_t, uint64_t, uint64_t,
                   uint64_t, uint64_t, uint64_t);

/**
 * @brief sys_getsid - Get session ID
 *
 * POSIX: pid_t getsid(pid_t pid)
 *
 * @param pid Process ID (or 0 for current)
 * @return SID on success, negative error code on failure
 */
int64_t sys_getsid(uint64_t pid,
                   uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

} // namespace xinim::kernel

#endif // XINIM_KERNEL_PROCESS_GROUP_HPP
