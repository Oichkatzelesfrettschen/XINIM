/**
 * @file process_group.cpp
 * @brief Process groups and sessions implementation
 *
 * Implements POSIX process groups, sessions, and job control.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "process_group.hpp"
#include "pcb.hpp"
#include "scheduler.hpp"
#include "signal.hpp"
#include "early/serial_16550.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// Global State
// ============================================================================

static ProcessGroup* g_process_groups_head = nullptr;
static Session* g_sessions_head = nullptr;

// ============================================================================
// Process Group Management
// ============================================================================

/**
 * @brief Initialize process group subsystem
 */
void init_process_groups() {
    g_process_groups_head = nullptr;
    g_sessions_head = nullptr;

    early_serial.write("[PGID] Process group subsystem initialized\n");
}

/**
 * @brief Create a new process group
 */
ProcessGroup* create_process_group(xinim::pid_t pgid, xinim::pid_t sid) {
    // Allocate process group
    ProcessGroup* pg = new ProcessGroup;
    if (!pg) {
        return nullptr;
    }

    pg->pgid = pgid;
    pg->session_id = sid;
    pg->members = nullptr;
    pg->next = g_process_groups_head;
    pg->prev = nullptr;
    pg->member_count = 0;
    pg->is_foreground = false;

    if (g_process_groups_head) {
        g_process_groups_head->prev = pg;
    }
    g_process_groups_head = pg;

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[PGID] Created process group %d in session %d\n",
                  pgid, sid);
    early_serial.write(buffer);

    return pg;
}

/**
 * @brief Find process group by PGID
 */
ProcessGroup* find_process_group(xinim::pid_t pgid) {
    ProcessGroup* pg = g_process_groups_head;
    while (pg) {
        if (pg->pgid == pgid) {
            return pg;
        }
        pg = pg->next;
    }
    return nullptr;
}

/**
 * @brief Add process to process group
 */
void add_to_process_group(ProcessControlBlock* pcb, ProcessGroup* pg) {
    if (!pcb || !pg) return;

    // Update PCB
    pcb->pgid = pg->pgid;
    pcb->pg_next = pg->members;
    pcb->pg_prev = nullptr;

    if (pg->members) {
        pg->members->pg_prev = pcb;
    }
    pg->members = pcb;
    pg->member_count++;

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[PGID] Process %d joined process group %d (count=%d)\n",
                  pcb->pid, pg->pgid, pg->member_count);
    early_serial.write(buffer);
}

/**
 * @brief Remove process from its process group
 */
void remove_from_process_group(ProcessControlBlock* pcb) {
    if (!pcb) return;

    ProcessGroup* pg = find_process_group(pcb->pgid);
    if (!pg) return;

    // Remove from linked list
    if (pcb->pg_prev) {
        pcb->pg_prev->pg_next = pcb->pg_next;
    } else {
        pg->members = pcb->pg_next;
    }

    if (pcb->pg_next) {
        pcb->pg_next->pg_prev = pcb->pg_prev;
    }

    pg->member_count--;
    pcb->pg_next = pcb->pg_prev = nullptr;

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[PGID] Process %d left process group %d (count=%d)\n",
                  pcb->pid, pg->pgid, pg->member_count);
    early_serial.write(buffer);

    // Delete process group if empty
    if (pg->member_count == 0) {
        delete_process_group(pg);
    }
}

/**
 * @brief Delete process group
 */
void delete_process_group(ProcessGroup* pg) {
    if (!pg) return;

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[PGID] Deleting process group %d\n", pg->pgid);
    early_serial.write(buffer);

    // Remove from global list
    if (pg->prev) {
        pg->prev->next = pg->next;
    } else {
        g_process_groups_head = pg->next;
    }

    if (pg->next) {
        pg->next->prev = pg->prev;
    }

    // If this was the foreground group, clear it
    Session* session = find_session(pg->session_id);
    if (session && session->foreground_group == pg) {
        session->foreground_group = nullptr;
    }

    delete pg;
}

/**
 * @brief Send signal to entire process group
 */
int signal_process_group(xinim::pid_t pgid, int sig) {
    ProcessGroup* pg = find_process_group(pgid);
    if (!pg) {
        return -ESRCH;  // No such process group
    }

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[PGID] Sending signal %d to process group %d (%d members)\n",
                  sig, pgid, pg->member_count);
    early_serial.write(buffer);

    int count = 0;
    ProcessControlBlock* pcb = pg->members;
    while (pcb) {
        send_signal(pcb, sig);
        count++;
        pcb = pcb->pg_next;
    }

    std::snprintf(buffer, sizeof(buffer),
                  "[PGID] Sent signal %d to %d processes\n", sig, count);
    early_serial.write(buffer);

    return 0;
}

// ============================================================================
// Session Management
// ============================================================================

/**
 * @brief Create a new session
 */
Session* create_session(xinim::pid_t sid) {
    // Allocate session
    Session* session = new Session;
    if (!session) {
        return nullptr;
    }

    session->sid = sid;
    session->groups = nullptr;
    session->foreground_group = nullptr;
    session->controlling_tty = nullptr;
    session->next = g_sessions_head;
    session->prev = nullptr;
    session->group_count = 0;

    if (g_sessions_head) {
        g_sessions_head->prev = session;
    }
    g_sessions_head = session;

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[SESSION] Created session %d\n", sid);
    early_serial.write(buffer);

    return session;
}

/**
 * @brief Find session by SID
 */
Session* find_session(xinim::pid_t sid) {
    Session* session = g_sessions_head;
    while (session) {
        if (session->sid == sid) {
            return session;
        }
        session = session->next;
    }
    return nullptr;
}

/**
 * @brief Delete session
 */
void delete_session(Session* session) {
    if (!session) return;

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[SESSION] Deleting session %d\n", session->sid);
    early_serial.write(buffer);

    // Remove from global list
    if (session->prev) {
        session->prev->next = session->next;
    } else {
        g_sessions_head = session->next;
    }

    if (session->next) {
        session->next->prev = session->prev;
    }

    delete session;
}

/**
 * @brief Set foreground process group
 */
int set_foreground_process_group(Session* session, ProcessGroup* pg) {
    if (!session) {
        return -EINVAL;
    }

    // Verify process group belongs to this session
    if (pg && pg->session_id != session->sid) {
        return -EINVAL;
    }

    // Clear old foreground group
    if (session->foreground_group) {
        session->foreground_group->is_foreground = false;
    }

    // Set new foreground group
    session->foreground_group = pg;
    if (pg) {
        pg->is_foreground = true;

        char buffer[128];
        std::snprintf(buffer, sizeof(buffer),
                      "[SESSION] Process group %d is now foreground in session %d\n",
                      pg->pgid, session->sid);
        early_serial.write(buffer);
    }

    return 0;
}

/**
 * @brief Get foreground process group
 */
ProcessGroup* get_foreground_process_group(Session* session) {
    if (!session) {
        return nullptr;
    }
    return session->foreground_group;
}

// ============================================================================
// Syscall Implementations
// ============================================================================

/**
 * @brief sys_setpgid - Set process group ID
 */
int64_t sys_setpgid(uint64_t pid, uint64_t pgid,
                    uint64_t, uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // pid == 0 means current process
    if (pid == 0) {
        pid = current->pid;
    }

    // pgid == 0 means use pid as pgid
    if (pgid == 0) {
        pgid = pid;
    }

    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
                  "[SYSCALL] sys_setpgid(%lu, %lu)\n", pid, pgid);
    early_serial.write(buffer);

    // Find target process
    ProcessControlBlock* target = find_process_by_pid((xinim::pid_t)pid);
    if (!target) {
        return -ESRCH;  // No such process
    }

    // Can only change own PGID or child's PGID before it execs
    if (target != current && target->parent_pid != current->pid) {
        return -EPERM;
    }

    // Process must be in same session
    if (target->sid != current->sid) {
        return -EPERM;
    }

    // Remove from old process group
    if (target->pgid != 0) {
        remove_from_process_group(target);
    }

    // Find or create new process group
    ProcessGroup* pg = find_process_group((xinim::pid_t)pgid);
    if (!pg) {
        // Create new process group
        pg = create_process_group((xinim::pid_t)pgid, target->sid);
        if (!pg) {
            return -ENOMEM;
        }
    }

    // Verify process group is in same session
    if (pg->session_id != target->sid) {
        return -EPERM;
    }

    // Add to new process group
    add_to_process_group(target, pg);

    std::snprintf(buffer, sizeof(buffer),
                  "[SYSCALL] Process %lu moved to process group %lu\n",
                  pid, pgid);
    early_serial.write(buffer);

    return 0;
}

/**
 * @brief sys_getpgid - Get process group ID
 */
int64_t sys_getpgid(uint64_t pid,
                    uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // pid == 0 means current process
    if (pid == 0) {
        return (int64_t)current->pgid;
    }

    // Find target process
    ProcessControlBlock* target = find_process_by_pid((xinim::pid_t)pid);
    if (!target) {
        return -ESRCH;
    }

    return (int64_t)target->pgid;
}

/**
 * @brief sys_getpgrp - Get process group of calling process
 */
int64_t sys_getpgrp(uint64_t, uint64_t, uint64_t,
                    uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    return (int64_t)current->pgid;
}

/**
 * @brief sys_setsid - Create new session
 */
int64_t sys_setsid(uint64_t, uint64_t, uint64_t,
                   uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer),
                  "[SYSCALL] sys_setsid() for process %d\n", current->pid);
    early_serial.write(buffer);

    // Cannot create session if already a process group leader
    if (current->pid == current->pgid) {
        early_serial.write("[SETSID] Error: Already a process group leader\n");
        return -EPERM;
    }

    // Remove from old process group
    if (current->pgid != 0) {
        remove_from_process_group(current);
    }

    // Create new session
    Session* session = create_session(current->pid);
    if (!session) {
        return -ENOMEM;
    }

    // Create new process group (same ID as session)
    ProcessGroup* pg = create_process_group(current->pid, current->pid);
    if (!pg) {
        delete_session(session);
        return -ENOMEM;
    }

    // Update current process
    current->sid = current->pid;
    add_to_process_group(current, pg);

    std::snprintf(buffer, sizeof(buffer),
                  "[SETSID] Process %d is now session leader (SID=%d, PGID=%d)\n",
                  current->pid, current->sid, current->pgid);
    early_serial.write(buffer);

    return (int64_t)current->sid;
}

/**
 * @brief sys_getsid - Get session ID
 */
int64_t sys_getsid(uint64_t pid,
                   uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // pid == 0 means current process
    if (pid == 0) {
        return (int64_t)current->sid;
    }

    // Find target process
    ProcessControlBlock* target = find_process_by_pid((xinim::pid_t)pid);
    if (!target) {
        return -ESRCH;
    }

    return (int64_t)target->sid;
}

} // namespace xinim::kernel
