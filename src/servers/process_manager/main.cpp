/**
 * @file main.cpp
 * @brief Process Manager - Userspace process lifecycle server for XINIM
 *
 * The Process Manager is a critical userspace component that handles all
 * process lifecycle operations: fork, exec, wait, exit, signals, UID/GID.
 *
 * Architecture:
 * 1. Maintains global process table (all processes in system)
 * 2. Receives IPC messages from kernel (PROC_FORK, PROC_EXEC, etc.)
 * 3. Manages parent-child relationships, zombies, orphans
 * 4. Delivers signals to processes
 * 5. Enforces UID/GID permissions
 *
 * @ingroup servers
 */

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <array>
#include "../../../include/xinim/ipc/message_types.h"
#include "../../../include/xinim/ipc/proc_protocol.hpp"
#include "../../../include/sys/type.hpp"

/* Lattice IPC functions */
namespace lattice {
    extern "C" int lattice_connect(int src, int dst, int node_id);
    extern "C" int lattice_send(int src, int dst, const message* msg, int flags);
    extern "C" int lattice_recv(int pid, message* msg, int flags);
}

namespace xinim::proc_mgr {

using namespace xinim::ipc;

/* ========================================================================
 * Process Control Block (PCB)
 * ======================================================================== */

/**
 * @brief Process states
 */
enum class ProcessState : uint8_t {
    RUNNING,        /**< Currently executing or runnable */
    SLEEPING,       /**< Waiting for event */
    ZOMBIE,         /**< Exited but not yet reaped by parent */
    STOPPED,        /**< Stopped by signal (SIGSTOP, SIGTSTP) */
};

/**
 * @brief Signal disposition
 */
enum class SignalAction : uint8_t {
    DEFAULT,        /**< Default action (term/ignore/stop/cont) */
    IGNORE,         /**< Ignore signal */
    HANDLER,        /**< Call user handler */
};

/**
 * @brief Process Control Block
 *
 * Stores all information about a single process.
 */
struct ProcessControlBlock {
    /* Process identification */
    int32_t pid;                /**< Process ID */
    int32_t ppid;               /**< Parent process ID */
    int32_t pgid;               /**< Process group ID */
    int32_t sid;                /**< Session ID */

    /* Process state */
    ProcessState state;         /**< Current state */
    int32_t exit_code;          /**< Exit status (valid if ZOMBIE) */

    /* User/group */
    uint32_t uid;               /**< Real user ID */
    uint32_t euid;              /**< Effective user ID */
    uint32_t gid;               /**< Real group ID */
    uint32_t egid;              /**< Effective group ID */

    /* Parent-child relationships */
    std::vector<int32_t> children;  /**< Child PIDs */

    /* Signal management */
    std::array<SignalAction, 64> sig_actions;  /**< Signal dispositions */
    std::array<uint64_t, 64> sig_handlers;     /**< Userspace handlers */
    uint64_t sig_mask;          /**< Blocked signals */
    uint64_t sig_pending;       /**< Pending signals */

    /* Working directory */
    char cwd[256];              /**< Current working directory */

    /* Resource usage (TODO) */
    uint64_t user_time;         /**< User CPU time (ns) */
    uint64_t sys_time;          /**< System CPU time (ns) */

    ProcessControlBlock()
        : pid(0), ppid(0), pgid(0), sid(0),
          state(ProcessState::RUNNING), exit_code(0),
          uid(0), euid(0), gid(0), egid(0),
          sig_mask(0), sig_pending(0),
          user_time(0), sys_time(0) {
        std::strcpy(cwd, "/");
        sig_actions.fill(SignalAction::DEFAULT);
        sig_handlers.fill(0);
    }
};

/* ========================================================================
 * Process Table
 * ======================================================================== */

/**
 * @brief Global process table
 */
class ProcessTable {
private:
    std::unordered_map<int32_t, ProcessControlBlock> processes_;
    int32_t next_pid_;

public:
    ProcessTable() : next_pid_(1) {
        // PID 0 is kernel, PID 1 is init
        // PIDs 2-4 are servers (VFS, PROC_MGR, MEM_MGR)
        next_pid_ = 5;  // First user process gets PID 5
    }

    /**
     * @brief Allocate a new PID
     */
    int32_t allocate_pid() {
        return next_pid_++;
    }

    /**
     * @brief Create a new process
     */
    ProcessControlBlock* create_process(int32_t ppid, uint32_t uid, uint32_t gid) {
        int32_t pid = allocate_pid();
        ProcessControlBlock pcb;
        pcb.pid = pid;
        pcb.ppid = ppid;
        pcb.pgid = pid;  // Initially, process group is self
        pcb.sid = pid;   // Initially, session is self
        pcb.uid = uid;
        pcb.euid = uid;
        pcb.gid = gid;
        pcb.egid = gid;
        pcb.state = ProcessState::RUNNING;

        // Inherit CWD from parent
        if (ppid > 0) {
            auto* parent = get_process(ppid);
            if (parent) {
                std::strcpy(pcb.cwd, parent->cwd);
            }
        }

        processes_[pid] = pcb;

        // Add to parent's children list
        if (ppid > 0) {
            auto* parent = get_process(ppid);
            if (parent) {
                parent->children.push_back(pid);
            }
        }

        return &processes_[pid];
    }

    /**
     * @brief Get process by PID
     */
    ProcessControlBlock* get_process(int32_t pid) {
        auto it = processes_.find(pid);
        return (it != processes_.end()) ? &it->second : nullptr;
    }

    /**
     * @brief Remove process from table
     */
    void remove_process(int32_t pid) {
        processes_.erase(pid);
    }

    /**
     * @brief Find zombie child of parent
     */
    ProcessControlBlock* find_zombie_child(int32_t ppid, int32_t target_pid) {
        for (auto& [pid, pcb] : processes_) {
            if (pcb.ppid == ppid && pcb.state == ProcessState::ZOMBIE) {
                if (target_pid == -1 || target_pid == pid) {
                    return &pcb;
                }
            }
        }
        return nullptr;
    }

    /**
     * @brief Reparent orphaned children to init (PID 1)
     */
    void reparent_children(int32_t old_ppid) {
        for (auto& [pid, pcb] : processes_) {
            if (pcb.ppid == old_ppid) {
                pcb.ppid = 1;  // Reparent to init
            }
        }
    }
};

/* ========================================================================
 * Process Manager State
 * ======================================================================== */

/**
 * @brief Global process manager state
 */
struct ProcMgrState {
    ProcessTable proc_table;
};

static ProcMgrState g_state;

/* ========================================================================
 * Process Operations
 * ======================================================================== */

/**
 * @brief Handle PROC_FORK request
 */
static void handle_fork(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcForkRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<ProcForkResponse*>(&response.m_u);

    response.m_source = PROC_MGR_PID;
    response.m_type = PROC_REPLY;

    // Get parent process
    auto* parent = g_state.proc_table.get_process(req->parent_pid);
    if (!parent) {
        resp->child_pid = -1;
        resp->error = IPC_ESRCH;  // No such process
        return;
    }

    // Create child process (copy of parent)
    auto* child = g_state.proc_table.create_process(req->parent_pid,
                                                     parent->uid,
                                                     parent->gid);
    if (!child) {
        resp->child_pid = -1;
        resp->error = IPC_ENOMEM;
        return;
    }

    // Child inherits signal masks from parent
    child->sig_mask = parent->sig_mask;
    child->sig_actions = parent->sig_actions;
    child->sig_handlers = parent->sig_handlers;

    // TODO: Kernel needs to:
    // 1. Copy parent's page tables to child
    // 2. Copy parent's stack and registers
    // 3. Set child's return value to 0
    // 4. Set parent's return value to child_pid
    // 5. Schedule both processes

    resp->child_pid = child->pid;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle PROC_EXEC request
 */
static void handle_exec(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcExecRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<ProcExecResponse*>(&response.m_u);

    response.m_source = PROC_MGR_PID;
    response.m_type = PROC_REPLY;

    // Get process
    auto* proc = g_state.proc_table.get_process(req->caller_pid);
    if (!proc) {
        resp->result = -1;
        resp->error = IPC_ESRCH;
        return;
    }

    // TODO: Kernel needs to:
    // 1. Load executable from VFS (via IPC to VFS server)
    // 2. Parse ELF headers
    // 3. Map program sections into memory
    // 4. Set up new stack with argc/argv/envp
    // 5. Reset signal handlers to default
    // 6. Jump to entry point

    // Reset signal handlers (POSIX requirement)
    proc->sig_actions.fill(SignalAction::DEFAULT);
    proc->sig_handlers.fill(0);

    // On success, exec does not return
    // On error, return -1
    resp->result = -1;
    resp->error = IPC_ENOEXEC;  // Exec format error (TODO: implement)
}

/**
 * @brief Handle PROC_EXIT request
 */
static void handle_exit(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcExitRequest*>(&request.m_u);

    // Get process
    auto* proc = g_state.proc_table.get_process(req->pid);
    if (!proc) {
        return;  // Process doesn't exist, nothing to do
    }

    // Mark as zombie
    proc->state = ProcessState::ZOMBIE;
    proc->exit_code = req->exit_code;

    // Reparent children to init
    g_state.proc_table.reparent_children(req->pid);

    // TODO: Kernel needs to:
    // 1. Free process memory
    // 2. Close all file descriptors (via IPC to VFS)
    // 3. Send SIGCHLD to parent
    // 4. Remove from scheduler
    // 5. If parent is waiting, wake it up

    // Exit doesn't send a response (process is terminating)
}

/**
 * @brief Handle PROC_WAIT request
 */
static void handle_wait(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcWaitRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<const ProcWaitResponse*>(&response.m_u);

    response.m_source = PROC_MGR_PID;
    response.m_type = PROC_REPLY;

    // Get parent process
    auto* parent = g_state.proc_table.get_process(req->parent_pid);
    if (!parent) {
        resp->child_pid = -1;
        resp->error = IPC_ESRCH;
        return;
    }

    // Find zombie child
    auto* child = g_state.proc_table.find_zombie_child(req->parent_pid, req->target_pid);

    if (!child) {
        // No zombie child found
        if (req->options & 0x1) {  // WNOHANG
            resp->child_pid = 0;  // No child available
            resp->error = IPC_SUCCESS;
        } else {
            // TODO: Block parent until child exits
            resp->child_pid = -1;
            resp->error = IPC_ECHILD;  // No child processes
        }
        return;
    }

    // Reap zombie child
    resp->child_pid = child->pid;
    resp->exit_status = child->exit_code;
    resp->error = IPC_SUCCESS;

    // Remove from parent's children list
    auto& children = parent->children;
    children.erase(std::remove(children.begin(), children.end(), child->pid),
                   children.end());

    // Remove from process table
    g_state.proc_table.remove_process(child->pid);
}

/**
 * @brief Handle PROC_KILL request
 */
static void handle_kill(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcKillRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<ProcGenericResponse*>(&response.m_u);

    response.m_source = PROC_MGR_PID;
    response.m_type = PROC_REPLY;

    // Get target process
    auto* proc = g_state.proc_table.get_process(req->target_pid);
    if (!proc) {
        resp->result = -1;
        resp->error = IPC_ESRCH;
        return;
    }

    // TODO: Check permissions (sender UID must match target UID, unless root)

    // TODO: Deliver signal to process
    // For now, just mark as pending
    proc->sig_pending |= (1ULL << req->signal);

    resp->result = 0;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle PROC_GETPID request
 */
static void handle_getpid(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcGetpidRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<ProcGetpidResponse*>(&response.m_u);

    response.m_source = PROC_MGR_PID;
    response.m_type = PROC_REPLY;

    resp->pid = req->caller_pid;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle PROC_GETPPID request
 */
static void handle_getppid(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcGetppidRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<ProcGetppidResponse*>(&response.m_u);

    response.m_source = PROC_MGR_PID;
    response.m_type = PROC_REPLY;

    auto* proc = g_state.proc_table.get_process(req->caller_pid);
    if (!proc) {
        resp->ppid = -1;
        resp->error = IPC_ESRCH;
        return;
    }

    resp->ppid = proc->ppid;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle PROC_SIGACTION request
 */
static void handle_sigaction(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcSigactionRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<ProcSigactionResponse*>(&response.m_u);

    response.m_source = PROC_MGR_PID;
    response.m_type = PROC_REPLY;

    auto* proc = g_state.proc_table.get_process(req->caller_pid);
    if (!proc) {
        resp->result = -1;
        resp->error = IPC_ESRCH;
        return;
    }

    if (req->signal < 1 || req->signal > 63) {
        resp->result = -1;
        resp->error = IPC_EINVAL;
        return;
    }

    // Save old handler
    resp->old_handler = proc->sig_handlers[req->signal];

    // Set new handler
    if (req->handler == 0) {
        proc->sig_actions[req->signal] = SignalAction::DEFAULT;
    } else if (req->handler == 1) {
        proc->sig_actions[req->signal] = SignalAction::IGNORE;
    } else {
        proc->sig_actions[req->signal] = SignalAction::HANDLER;
        proc->sig_handlers[req->signal] = req->handler;
    }

    resp->result = 0;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle UID/GID requests
 */
static void handle_getuid(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcUidRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<ProcUidResponse*>(&response.m_u);

    response.m_source = PROC_MGR_PID;
    response.m_type = PROC_REPLY;

    auto* proc = g_state.proc_table.get_process(req->caller_pid);
    if (!proc) {
        resp->uid = -1;
        resp->error = IPC_ESRCH;
        return;
    }

    resp->uid = proc->uid;
    resp->result = proc->uid;
    resp->error = IPC_SUCCESS;
}

static void handle_geteuid(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcUidRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<ProcUidResponse*>(&response.m_u);

    response.m_source = PROC_MGR_PID;
    response.m_type = PROC_REPLY;

    auto* proc = g_state.proc_table.get_process(req->caller_pid);
    if (!proc) {
        resp->uid = -1;
        resp->error = IPC_ESRCH;
        return;
    }

    resp->uid = proc->euid;
    resp->result = proc->euid;
    resp->error = IPC_SUCCESS;
}

static void handle_setuid(const message& request, message& response) {
    const auto* req = reinterpret_cast<const ProcUidRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<ProcUidResponse*>(&response.m_u);

    response.m_source = PROC_MGR_PID;
    response.m_type = PROC_REPLY;

    auto* proc = g_state.proc_table.get_process(req->caller_pid);
    if (!proc) {
        resp->result = -1;
        resp->error = IPC_ESRCH;
        return;
    }

    // Only root (UID 0) can change UID arbitrarily
    if (proc->euid != 0 && req->new_uid != proc->uid) {
        resp->result = -1;
        resp->error = IPC_EPERM;
        return;
    }

    proc->uid = req->new_uid;
    proc->euid = req->new_uid;
    resp->result = 0;
    resp->error = IPC_SUCCESS;
}

/* ========================================================================
 * Message Dispatcher
 * ======================================================================== */

/**
 * @brief Dispatch incoming IPC message
 */
static void dispatch_message(const message& request, message& response) {
    switch (request.m_type) {
        case PROC_FORK:
            handle_fork(request, response);
            break;
        case PROC_EXEC:
            handle_exec(request, response);
            break;
        case PROC_EXIT:
            handle_exit(request, response);
            break;
        case PROC_WAIT:
            handle_wait(request, response);
            break;
        case PROC_KILL:
            handle_kill(request, response);
            break;
        case PROC_GETPID:
            handle_getpid(request, response);
            break;
        case PROC_GETPPID:
            handle_getppid(request, response);
            break;
        case PROC_SIGACTION:
            handle_sigaction(request, response);
            break;
        case PROC_GETUID:
            handle_getuid(request, response);
            break;
        case PROC_GETEUID:
            handle_geteuid(request, response);
            break;
        case PROC_SETUID:
            handle_setuid(request, response);
            break;
        default:
            response.m_source = PROC_MGR_PID;
            response.m_type = PROC_ERROR;
            auto* resp = reinterpret_cast<ProcGenericResponse*>(&response.m_u);
            resp->result = -1;
            resp->error = IPC_ENOSYS;
            break;
    }
}

/* ========================================================================
 * Server Initialization and Main Loop
 * ======================================================================== */

/**
 * @brief Initialize process manager
 */
static bool initialize() {
    // Create init process (PID 1) in process table
    auto* init = g_state.proc_table.create_process(0, 0, 0);
    if (!init) {
        return false;
    }
    init->pid = 1;  // Override allocated PID
    init->ppid = 0; // Parent is kernel

    // Create VFS server entry (PID 2)
    auto* vfs = g_state.proc_table.create_process(1, 0, 0);
    if (!vfs) {
        return false;
    }
    vfs->pid = 2;
    vfs->ppid = 1;

    // Create Process Manager entry (PID 3) - this server
    auto* procmgr = g_state.proc_table.create_process(1, 0, 0);
    if (!procmgr) {
        return false;
    }
    procmgr->pid = 3;
    procmgr->ppid = 1;

    // Create Memory Manager entry (PID 4)
    auto* memmgr = g_state.proc_table.create_process(1, 0, 0);
    if (!memmgr) {
        return false;
    }
    memmgr->pid = 4;
    memmgr->ppid = 1;

    return true;
}

/**
 * @brief Process manager main loop
 */
static void server_loop() {
    message request, response;

    while (true) {
        // Receive IPC message from kernel
        int result = lattice::lattice_recv(PROC_MGR_PID, &request, 0);
        if (result < 0) {
            continue;
        }

        // Dispatch request
        std::memset(&response, 0, sizeof(response));
        dispatch_message(request, response);

        // Send response back to caller (unless it's PROC_EXIT)
        if (request.m_type != PROC_EXIT) {
            lattice::lattice_send(PROC_MGR_PID, request.m_source, &response, 0);
        }
    }
}

} // namespace xinim::proc_mgr

/* ========================================================================
 * Entry Point
 * ======================================================================== */

/**
 * @brief Process Manager entry point
 */
int main() {
    using namespace xinim::proc_mgr;

    if (!initialize()) {
        return 1;
    }

    server_loop();

    return 0;
}

/**
 * @brief C-compatible entry point for kernel spawn
 *
 * This wrapper allows the kernel to spawn the Process Manager during boot.
 */
extern "C" void proc_mgr_main() {
    main();  // Call C++ main

    // Server should never exit
    for(;;) {
#ifdef XINIM_ARCH_X86_64
        __asm__ volatile ("hlt");
#elif defined(XINIM_ARCH_ARM64)
        __asm__ volatile ("wfi");
#endif
    }
}
