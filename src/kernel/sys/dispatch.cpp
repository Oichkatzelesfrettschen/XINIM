#include <stdint.h>
#include <string.h>
#include <xinim/sys/syscalls.h>
#include "../../../include/xinim/ipc/message_types.h"
#include "../../../include/xinim/ipc/vfs_protocol.hpp"
#include "../../../include/xinim/ipc/proc_protocol.hpp"
#include "../../../include/xinim/ipc/mm_protocol.hpp"
#include "../../../include/sys/type.hpp"
#include "../early/serial_16550.hpp"
#include "../time/monotonic.hpp"
#include "../lattice_ipc.hpp"

extern xinim::early::Serial16550 early_serial;

// ============================================================================
// IPC Helper Functions
// ============================================================================

/**
 * @brief Send IPC request to a server and wait for response
 * @param caller_pid PID of calling process
 * @param server_pid PID of target server
 * @param request Request message
 * @param response Response message buffer
 * @return 0 on success, -1 on error
 */
static int send_ipc_request(xinim::pid_t caller_pid, xinim::pid_t server_pid,
                             const message& request, message& response) {
    // Send request
    int result = lattice::lattice_send(caller_pid, server_pid, request, lattice::IpcFlags::NONE);
    if (result != OK) {
        return -1;
    }

    // Wait for response
    result = lattice::lattice_recv(caller_pid, &response, lattice::IpcFlags::NONE);
    if (result != OK) {
        return -1;
    }

    return 0;
}

// ============================================================================
// Debug/Legacy Syscalls
// ============================================================================

static uint64_t sys_debug_write_impl(const char* s, uint64_t n) {
    for (uint64_t i=0;i<n;i++) early_serial.write_char(s[i]);
    return n;
}

static uint64_t sys_monotonic_ns_impl() {
    return xinim::time::monotonic_ns();
}

// ============================================================================
// File I/O Syscalls (delegated to VFS server via lattice IPC)
// ============================================================================

static int64_t sys_open_impl(const char* path, int flags, int mode) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();  // TODO: Get actual PID
    request.m_type = VFS_OPEN;

    auto* req = reinterpret_cast<VfsOpenRequest*>(&request.m_u);
    *req = VfsOpenRequest(path, flags, mode, request.m_source);

    // Send to VFS server
    if (send_ipc_request(request.m_source, VFS_SERVER_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const VfsOpenResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    return resp->fd;
}

static int64_t sys_close_impl(int fd) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = VFS_CLOSE;

    auto* req = reinterpret_cast<VfsCloseRequest*>(&request.m_u);
    *req = VfsCloseRequest(fd, request.m_source);

    // Send to VFS server
    if (send_ipc_request(request.m_source, VFS_SERVER_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const VfsCloseResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? 0 : -1;
}

static int64_t sys_read_impl(int fd, void* buf, uint64_t count) {
    using namespace xinim::ipc;

    // Special case: fd 0 (stdin) - for now, return 0 (EOF)
    if (fd == 0) return 0;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = VFS_READ;

    auto* req = reinterpret_cast<VfsReadRequest*>(&request.m_u);
    *req = VfsReadRequest(fd, count, -1, request.m_source);  // -1 = use fd offset

    // Send to VFS server
    if (send_ipc_request(request.m_source, VFS_SERVER_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const VfsReadResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    // Copy inline data to user buffer (for small reads)
    if (resp->bytes_read > 0 && resp->bytes_read <= 256) {
        memcpy(buf, resp->inline_data, resp->bytes_read);
    }

    return resp->bytes_read;
}

static int64_t sys_write_impl(int fd, const void* buf, uint64_t count) {
    using namespace xinim::ipc;

    // Special case: fd 1 (stdout) and fd 2 (stderr) - write to serial
    if (fd == 1 || fd == 2) {
        return sys_debug_write_impl((const char*)buf, count);
    }

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = VFS_WRITE;

    auto* req = reinterpret_cast<VfsWriteRequest*>(&request.m_u);
    req->fd = fd;
    req->count = count;
    req->offset = -1;  // Use fd offset
    req->caller_pid = request.m_source;

    // Copy inline data (for small writes)
    if (count <= 256) {
        memcpy(req->inline_data, buf, count);
    }

    // Send to VFS server
    if (send_ipc_request(request.m_source, VFS_SERVER_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const VfsWriteResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    return resp->bytes_written;
}

static int64_t sys_lseek_impl(int fd, int64_t offset, int whence) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = VFS_LSEEK;

    auto* req = reinterpret_cast<VfsLseekRequest*>(&request.m_u);
    *req = VfsLseekRequest(fd, offset, whence, request.m_source);

    // Send to VFS server
    if (send_ipc_request(request.m_source, VFS_SERVER_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const VfsLseekResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    return resp->new_offset;
}

static int64_t sys_stat_impl(const char* path, void* statbuf) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = VFS_STAT;

    auto* req = reinterpret_cast<VfsStatRequest*>(&request.m_u);
    strncpy(req->path, path, sizeof(req->path) - 1);
    req->path[sizeof(req->path) - 1] = '\0';
    req->caller_pid = request.m_source;
    req->is_fstat = false;

    // Send to VFS server
    if (send_ipc_request(request.m_source, VFS_SERVER_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const VfsStatResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    // Copy stat info to user buffer
    if (statbuf) {
        memcpy(statbuf, &resp->stat, sizeof(VfsStatInfo));
    }

    return 0;
}

static int64_t sys_fstat_impl(int fd, void* statbuf) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = VFS_FSTAT;

    auto* req = reinterpret_cast<VfsStatRequest*>(&request.m_u);
    req->fd = fd;
    req->caller_pid = request.m_source;
    req->is_fstat = true;

    // Send to VFS server
    if (send_ipc_request(request.m_source, VFS_SERVER_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const VfsStatResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    // Copy stat info to user buffer
    if (statbuf) {
        memcpy(statbuf, &resp->stat, sizeof(VfsStatInfo));
    }

    return 0;
}

static int64_t sys_access_impl(const char* path, int mode) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_dup_impl(int fd) {
    // TODO: Implement in process table
    return -1;
}

static int64_t sys_dup2_impl(int oldfd, int newfd) {
    // TODO: Implement in process table
    return -1;
}

static int64_t sys_pipe_impl(int pipefd[2]) {
    // TODO: Create pipe via VFS server
    return -1;
}

static int64_t sys_ioctl_impl(int fd, uint64_t request, void* argp) {
    // TODO: Send IPC message to device driver
    return -1;
}

static int64_t sys_fcntl_impl(int fd, int cmd, uint64_t arg) {
    // TODO: Implement in process table
    return -1;
}

// ============================================================================
// Directory Operations
// ============================================================================

static int64_t sys_mkdir_impl(const char* path, int mode) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = VFS_MKDIR;

    auto* req = reinterpret_cast<VfsMkdirRequest*>(&request.m_u);
    *req = VfsMkdirRequest(path, mode, request.m_source);

    // Send to VFS server
    if (send_ipc_request(request.m_source, VFS_SERVER_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const VfsMkdirResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? 0 : -1;
}

static int64_t sys_rmdir_impl(const char* path) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = VFS_RMDIR;

    auto* req = reinterpret_cast<VfsRmdirRequest*>(&request.m_u);
    strncpy(req->path, path, sizeof(req->path) - 1);
    req->path[sizeof(req->path) - 1] = '\0';
    req->caller_pid = request.m_source;

    // Send to VFS server
    if (send_ipc_request(request.m_source, VFS_SERVER_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const VfsRmdirResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? 0 : -1;
}

static int64_t sys_chdir_impl(const char* path) {
    // TODO: Update current process working directory in process table
    // For now, delegate to VFS server for validation
    return -1;
}

static int64_t sys_getcwd_impl(char* buf, uint64_t size) {
    // TODO: Read from current process working directory in process table
    return -1;
}

static int64_t sys_link_impl(const char* oldpath, const char* newpath) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_unlink_impl(const char* path) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = VFS_UNLINK;

    auto* req = reinterpret_cast<VfsUnlinkRequest*>(&request.m_u);
    strncpy(req->path, path, sizeof(req->path) - 1);
    req->path[sizeof(req->path) - 1] = '\0';
    req->caller_pid = request.m_source;

    // Send to VFS server
    if (send_ipc_request(request.m_source, VFS_SERVER_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const VfsGenericResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? 0 : -1;
}

static int64_t sys_rename_impl(const char* oldpath, const char* newpath) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_chmod_impl(const char* path, int mode) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_chown_impl(const char* path, int uid, int gid) {
    // TODO: Send IPC message to VFS server
    return -1;
}

// ============================================================================
// Process Management (delegated to Process Manager via lattice IPC)
// ============================================================================

static void sys_exit_impl(int status) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_EXIT;

    auto* req = reinterpret_cast<ProcExitRequest*>(&request.m_u);
    req->pid = request.m_source;
    req->exit_code = status;

    // Send to Process Manager (no response expected)
    lattice::lattice_send(request.m_source, PROC_MGR_PID, request, lattice::IpcFlags::NONE);

    // TODO: Kernel cleanup:
    // - Free process memory
    // - Close file descriptors
    // - Remove from scheduler
    // - Send SIGCHLD to parent

    // Loop forever (process should be removed from scheduler)
    while (1) {}
}

static int64_t sys_fork_impl() {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_FORK;

    auto* req = reinterpret_cast<ProcForkRequest*>(&request.m_u);
    req->parent_pid = request.m_source;
    // TODO: Save parent registers (rsp, rip, rflags) from syscall context
    req->parent_rsp = 0;
    req->parent_rip = 0;
    req->parent_rflags = 0;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcForkResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    // TODO: Kernel needs to:
    // 1. Copy parent's page tables to child
    // 2. Copy parent's FD table (via VFS IPC)
    // 3. Set child's return value to 0
    // 4. Add both processes to scheduler

    return resp->child_pid;  // Parent gets child PID, child gets 0
}

static int64_t sys_execve_impl(const char* filename, char* const argv[], char* const envp[]) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_EXEC;

    auto* req = reinterpret_cast<ProcExecRequest*>(&request.m_u);
    std::strncpy(req->path, filename, sizeof(req->path) - 1);
    req->path[sizeof(req->path) - 1] = '\0';
    req->argc = 0;  // TODO: Count argv
    req->envc = 0;  // TODO: Count envp
    req->caller_pid = request.m_source;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return -1;
    }

    // TODO: Kernel needs to:
    // 1. Load ELF binary from VFS
    // 2. Parse ELF headers
    // 3. Unmap old address space (via Memory Manager)
    // 4. Map new sections (via Memory Manager)
    // 5. Set up stack with argc/argv/envp
    // 6. Jump to entry point

    // exec() only returns on error
    const auto* resp = reinterpret_cast<const ProcExecResponse*>(&response.m_u);
    return resp->result;
}

static int64_t sys_wait4_impl(int pid, int* status, int options, void* rusage) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_WAIT;

    auto* req = reinterpret_cast<ProcWaitRequest*>(&request.m_u);
    req->parent_pid = request.m_source;
    req->target_pid = pid;
    req->options = options;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcWaitResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    // Copy exit status to user buffer
    if (status) {
        *status = resp->exit_status;
    }

    // TODO: Copy rusage if requested

    return resp->child_pid;
}

static int64_t sys_getpid_impl() {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = 1;  // Bootstrap: assume PID 1 until we have proper context
    request.m_type = PROC_GETPID;

    auto* req = reinterpret_cast<ProcGetpidRequest*>(&request.m_u);
    req->caller_pid = request.m_source;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return 1;  // Fallback
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcGetpidResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? resp->pid : 1;
}

static int64_t sys_getppid_impl() {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_GETPPID;

    auto* req = reinterpret_cast<ProcGetppidRequest*>(&request.m_u);
    req->caller_pid = request.m_source;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcGetppidResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? resp->ppid : -1;
}

static int64_t sys_kill_impl(int pid, int sig) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_KILL;

    auto* req = reinterpret_cast<ProcKillRequest*>(&request.m_u);
    req->target_pid = pid;
    req->signal = sig;
    req->sender_pid = request.m_source;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcGenericResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? 0 : -1;
}

static int64_t sys_signal_impl(int signum, uint64_t handler) {
    using namespace xinim::ipc;

    // Prepare IPC request (signal() is simplified sigaction)
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_SIGACTION;

    auto* req = reinterpret_cast<ProcSigactionRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->signal = signum;
    req->handler = handler;
    req->sigaction_flags = 0;
    req->sa_mask = 0;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcSigactionResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? resp->old_handler : -1;
}

static int64_t sys_sigaction_impl(int signum, const void* act, void* oldact) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_SIGACTION;

    auto* req = reinterpret_cast<ProcSigactionRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->signal = signum;

    // TODO: Parse struct sigaction from userspace
    // For now, use simplified version
    req->handler = 0;
    req->sigaction_flags = 0;
    req->sa_mask = 0;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcSigactionResponse*>(&response.m_u);

    // TODO: Copy old sigaction to userspace if oldact != NULL

    return (resp->error == IPC_SUCCESS) ? 0 : -1;
}

// ============================================================================
// Memory Management (delegated to Memory Manager via lattice IPC)
// ============================================================================

static int64_t sys_brk_impl(void* addr) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = MM_BRK;

    auto* req = reinterpret_cast<MmBrkRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->new_brk = reinterpret_cast<uint64_t>(addr);

    // Send to Memory Manager
    if (send_ipc_request(request.m_source, MEM_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const MmBrkResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    // TODO: Kernel needs to allocate/free pages based on brk change

    return resp->current_brk;
}

static int64_t sys_mmap_impl(void* addr, uint64_t length, int prot, int flags, int fd, int64_t offset) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = MM_MMAP;

    auto* req = reinterpret_cast<MmMmapRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->addr = reinterpret_cast<uint64_t>(addr);
    req->length = length;
    req->prot = prot;
    req->flags = flags;
    req->fd = fd;
    req->offset = offset;

    // Send to Memory Manager
    if (send_ipc_request(request.m_source, MEM_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const MmMmapResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    // TODO: Kernel needs to:
    // 1. Allocate page table entries
    // 2. If MAP_ANONYMOUS, allocate pages
    // 3. If file-backed, set up demand paging
    // 4. Set permissions in page table

    return resp->mapped_addr;
}

static int64_t sys_munmap_impl(void* addr, uint64_t length) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = MM_MUNMAP;

    auto* req = reinterpret_cast<MmMunmapRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->addr = reinterpret_cast<uint64_t>(addr);
    req->length = length;

    // Send to Memory Manager
    if (send_ipc_request(request.m_source, MEM_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const MmMunmapResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    // TODO: Kernel needs to:
    // 1. Unmap pages from page table
    // 2. Free physical pages
    // 3. Flush TLB

    return 0;
}

static int64_t sys_mprotect_impl(void* addr, uint64_t length, int prot) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = MM_MPROTECT;

    auto* req = reinterpret_cast<MmMprotectRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->addr = reinterpret_cast<uint64_t>(addr);
    req->length = length;
    req->prot = prot;

    // Send to Memory Manager
    if (send_ipc_request(request.m_source, MEM_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const MmMprotectResponse*>(&response.m_u);
    if (resp->error != IPC_SUCCESS) {
        return -1;
    }

    // TODO: Kernel needs to:
    // 1. Update page table entry permissions
    // 2. Flush TLB

    return 0;
}

// ============================================================================
// IPC (XINIM Lattice IPC)
// ============================================================================

static int64_t sys_lattice_connect_impl(const char* name, int flags, int mode) {
    // TODO: Connect to lattice IPC endpoint
    return -1;
}

static int64_t sys_lattice_send_impl(int endpoint_id, const void* msg, uint64_t len, int prio) {
    // TODO: Send message via lattice IPC
    return -1;
}

static int64_t sys_lattice_recv_impl(int endpoint_id, void* msg, uint64_t len, int* prio) {
    // TODO: Receive message via lattice IPC
    return -1;
}

static int64_t sys_lattice_close_impl(int endpoint_id) {
    // TODO: Close lattice IPC endpoint
    return -1;
}

// ============================================================================
// Time
// ============================================================================

static int64_t sys_time_impl(int64_t* tloc) {
    // TODO: Get POSIX time from RTC
    int64_t t = xinim::time::monotonic_ns() / 1000000000; // Convert ns to seconds (stub)
    if (tloc) *tloc = t;
    return t;
}

static int64_t sys_gettimeofday_impl(void* tv, void* tz) {
    // TODO: Get time of day from RTC
    return -1;
}

static int64_t sys_clock_gettime_impl(int clk_id, void* tp) {
    // TODO: Get clock time (CLOCK_REALTIME, CLOCK_MONOTONIC)
    return -1;
}

static int64_t sys_nanosleep_impl(const void* req, void* rem) {
    // TODO: Sleep for specified time via scheduler
    return -1;
}

// ============================================================================
// User/Group IDs (delegated to Process Manager via lattice IPC)
// ============================================================================

static int64_t sys_getuid_impl() {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_GETUID;

    auto* req = reinterpret_cast<ProcUidRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->new_uid = -1;  // -1 means "get", not "set"

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return 0;  // Fallback to root
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcUidResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? resp->uid : 0;
}

static int64_t sys_geteuid_impl() {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_GETEUID;

    auto* req = reinterpret_cast<ProcUidRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->new_uid = -1;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return 0;  // Fallback to root
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcUidResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? resp->uid : 0;
}

static int64_t sys_getgid_impl() {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_GETGID;

    auto* req = reinterpret_cast<ProcGidRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->new_gid = -1;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return 0;  // Fallback to root
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcGidResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? resp->gid : 0;
}

static int64_t sys_getegid_impl() {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_GETEGID;

    auto* req = reinterpret_cast<ProcGidRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->new_gid = -1;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return 0;  // Fallback to root
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcGidResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? resp->gid : 0;
}

static int64_t sys_setuid_impl(int uid) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_SETUID;

    auto* req = reinterpret_cast<ProcUidRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->new_uid = uid;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcUidResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? 0 : -1;
}

static int64_t sys_setgid_impl(int gid) {
    using namespace xinim::ipc;

    // Prepare IPC request
    message request{}, response{};
    request.m_source = sys_getpid_impl();
    request.m_type = PROC_SETGID;

    auto* req = reinterpret_cast<ProcGidRequest*>(&request.m_u);
    req->caller_pid = request.m_source;
    req->new_gid = gid;

    // Send to Process Manager
    if (send_ipc_request(request.m_source, PROC_MGR_PID, request, response) != 0) {
        return -1;
    }

    // Parse response
    const auto* resp = reinterpret_cast<const ProcGidResponse*>(&response.m_u);
    return (resp->error == IPC_SUCCESS) ? 0 : -1;
}

// ============================================================================
// Syscall Dispatcher
// ============================================================================

extern "C" uint64_t xinim_syscall_dispatch(uint64_t no,
    uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    switch (no) {
        // Debug/Legacy
        case SYS_debug_write: return sys_debug_write_impl((const char*)a0, a1);
        case SYS_monotonic_ns: return sys_monotonic_ns_impl();

        // File I/O
        case SYS_open: return sys_open_impl((const char*)a0, a1, a2);
        case SYS_close: return sys_close_impl(a0);
        case SYS_read: return sys_read_impl(a0, (void*)a1, a2);
        case SYS_write: return sys_write_impl(a0, (const void*)a1, a2);
        case SYS_lseek: return sys_lseek_impl(a0, a1, a2);
        case SYS_stat: return sys_stat_impl((const char*)a0, (void*)a1);
        case SYS_fstat: return sys_fstat_impl(a0, (void*)a1);
        case SYS_access: return sys_access_impl((const char*)a0, a1);
        case SYS_dup: return sys_dup_impl(a0);
        case SYS_dup2: return sys_dup2_impl(a0, a1);
        case SYS_pipe: return sys_pipe_impl((int*)a0);
        case SYS_ioctl: return sys_ioctl_impl(a0, a1, (void*)a2);
        case SYS_fcntl: return sys_fcntl_impl(a0, a1, a2);

        // Directory operations
        case SYS_mkdir: return sys_mkdir_impl((const char*)a0, a1);
        case SYS_rmdir: return sys_rmdir_impl((const char*)a0);
        case SYS_chdir: return sys_chdir_impl((const char*)a0);
        case SYS_getcwd: return sys_getcwd_impl((char*)a0, a1);
        case SYS_link: return sys_link_impl((const char*)a0, (const char*)a1);
        case SYS_unlink: return sys_unlink_impl((const char*)a0);
        case SYS_rename: return sys_rename_impl((const char*)a0, (const char*)a1);
        case SYS_chmod: return sys_chmod_impl((const char*)a0, a1);
        case SYS_chown: return sys_chown_impl((const char*)a0, a1, a2);

        // Process management
        case SYS_exit: sys_exit_impl(a0); return 0; // Never returns
        case SYS_fork: return sys_fork_impl();
        case SYS_execve: return sys_execve_impl((const char*)a0, (char* const*)a1, (char* const*)a2);
        case SYS_wait4: return sys_wait4_impl(a0, (int*)a1, a2, (void*)a3);
        case SYS_getpid: return sys_getpid_impl();
        case SYS_getppid: return sys_getppid_impl();
        case SYS_kill: return sys_kill_impl(a0, a1);
        case SYS_signal: return sys_signal_impl(a0, a1);
        case SYS_sigaction: return sys_sigaction_impl(a0, (const void*)a1, (void*)a2);

        // Memory management
        case SYS_brk: return sys_brk_impl((void*)a0);
        case SYS_mmap: return sys_mmap_impl((void*)a0, a1, a2, a3, a4, (int64_t)a4);
        case SYS_munmap: return sys_munmap_impl((void*)a0, a1);
        case SYS_mprotect: return sys_mprotect_impl((void*)a0, a1, a2);

        // IPC (XINIM lattice)
        case SYS_lattice_connect: return sys_lattice_connect_impl((const char*)a0, a1, a2);
        case SYS_lattice_send: return sys_lattice_send_impl(a0, (const void*)a1, a2, a3);
        case SYS_lattice_recv: return sys_lattice_recv_impl(a0, (void*)a1, a2, (int*)a3);
        case SYS_lattice_close: return sys_lattice_close_impl(a0);

        // Time
        case SYS_time: return sys_time_impl((int64_t*)a0);
        case SYS_gettimeofday: return sys_gettimeofday_impl((void*)a0, (void*)a1);
        case SYS_clock_gettime: return sys_clock_gettime_impl(a0, (void*)a1);
        case SYS_nanosleep: return sys_nanosleep_impl((const void*)a0, (void*)a1);

        // User/Group IDs
        case SYS_getuid: return sys_getuid_impl();
        case SYS_geteuid: return sys_geteuid_impl();
        case SYS_getgid: return sys_getgid_impl();
        case SYS_getegid: return sys_getegid_impl();
        case SYS_setuid: return sys_setuid_impl(a0);
        case SYS_setgid: return sys_setgid_impl(a0);

        default: return (uint64_t)-1; // ENOSYS
    }
}
