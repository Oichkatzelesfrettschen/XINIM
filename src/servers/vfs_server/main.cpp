/**
 * @file main.cpp
 * @brief VFS Server - Userspace filesystem server for XINIM
 *
 * The VFS server is a critical userspace component that handles all
 * file I/O operations for XINIM. It receives syscall requests from
 * the kernel via Lattice IPC, performs the requested operations on
 * the ramfs filesystem, and sends responses back.
 *
 * Architecture:
 * 1. Initialize ramfs (in-memory filesystem)
 * 2. Register with kernel as VFS_SERVER_PID (2)
 * 3. Enter message loop
 * 4. Receive IPC messages (VFS_OPEN, VFS_READ, VFS_WRITE, etc.)
 * 5. Dispatch to ramfs operations
 * 6. Send IPC responses back to kernel
 *
 * @ingroup servers
 */

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include "../../../include/xinim/ipc/message_types.h"
#include "../../../include/xinim/ipc/vfs_protocol.hpp"
#include "../../vfs/ramfs.hpp"
#include "../../vfs/path_util.hpp"
#include "../../../include/sys/type.hpp"

/* For now, we'll use stub IPC functions until we integrate with the kernel */
namespace lattice {
    extern "C" int lattice_connect(int src, int dst, int node_id);
    extern "C" int lattice_send(int src, int dst, const message* msg, int flags);
    extern "C" int lattice_recv(int pid, message* msg, int flags);
}

namespace xinim::vfs_server {

using namespace xinim::vfs;
using namespace xinim::ipc;

/* ========================================================================
 * File Descriptor Table
 * ======================================================================== */

/**
 * @brief File descriptor entry
 */
struct FileDescriptor {
    std::shared_ptr<RamfsNode> node;  /**< File node */
    uint64_t offset;                  /**< Current file offset */
    int flags;                        /**< Open flags */

    FileDescriptor() : offset(0), flags(0) {}
    FileDescriptor(std::shared_ptr<RamfsNode> n, int f)
        : node(n), offset(0), flags(f) {}
};

/**
 * @brief Per-process file descriptor table
 */
class FdTable {
private:
    std::unordered_map<int, FileDescriptor> fds_;
    int next_fd_;

public:
    FdTable() : next_fd_(3) {}  // 0, 1, 2 are stdin, stdout, stderr

    int allocate_fd(std::shared_ptr<RamfsNode> node, int flags) {
        int fd = next_fd_++;
        fds_[fd] = FileDescriptor(node, flags);
        return fd;
    }

    FileDescriptor* get_fd(int fd) {
        auto it = fds_.find(fd);
        return (it != fds_.end()) ? &it->second : nullptr;
    }

    bool close_fd(int fd) {
        return fds_.erase(fd) > 0;
    }
};

/* ========================================================================
 * VFS Server State
 * ======================================================================== */

/**
 * @brief Global VFS server state
 */
struct VfsServerState {
    RamfsFilesystem fs;                           /**< Ramfs filesystem */
    std::unordered_map<int, FdTable> process_fds; /**< Per-process FD tables */

    FdTable* get_process_fds(int pid) {
        auto it = process_fds.find(pid);
        if (it == process_fds.end()) {
            process_fds[pid] = FdTable();
            return &process_fds[pid];
        }
        return &it->second;
    }
};

static VfsServerState g_state;

/* ========================================================================
 * VFS Operations
 * ======================================================================== */

/**
 * @brief Handle VFS_OPEN request
 */
static void handle_open(const message& request, message& response) {
    const auto* req = reinterpret_cast<const VfsOpenRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<VfsOpenResponse*>(&response.m_u);

    response.m_source = VFS_SERVER_PID;
    response.m_type = VFS_REPLY;

    // Resolve path
    auto node = resolve_path(g_state.fs, req->path);

    // If node doesn't exist and O_CREAT is set, create it
    if (!node && (req->flags & 0x40)) {  // O_CREAT = 0x40
        auto [parent, filename] = resolve_parent(g_state.fs, req->path);
        if (parent) {
            node = g_state.fs.create_file(parent, filename, req->mode & 0x0FFF);
        }
    }

    if (!node) {
        resp->fd = -1;
        resp->error = IPC_ENOENT;
        return;
    }

    // Check permissions (TODO: get actual UID/GID from process)
    if (!node->can_read(0, 0)) {
        resp->fd = -1;
        resp->error = IPC_EACCES;
        return;
    }

    // Allocate file descriptor
    FdTable* fds = g_state.get_process_fds(req->caller_pid);
    int fd = fds->allocate_fd(node, req->flags);

    resp->fd = fd;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle VFS_CLOSE request
 */
static void handle_close(const message& request, message& response) {
    const auto* req = reinterpret_cast<const VfsCloseRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<VfsCloseResponse*>(&response.m_u);

    response.m_source = VFS_SERVER_PID;
    response.m_type = VFS_REPLY;

    FdTable* fds = g_state.get_process_fds(req->caller_pid);
    bool success = fds->close_fd(req->fd);

    resp->result = success ? 0 : -1;
    resp->error = success ? IPC_SUCCESS : IPC_EBADF;
}

/**
 * @brief Handle VFS_READ request
 */
static void handle_read(const message& request, message& response) {
    const auto* req = reinterpret_cast<const VfsReadRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<VfsReadResponse*>(&response.m_u);

    response.m_source = VFS_SERVER_PID;
    response.m_type = VFS_REPLY;

    FdTable* fds = g_state.get_process_fds(req->caller_pid);
    FileDescriptor* fd_entry = fds->get_fd(req->fd);

    if (!fd_entry) {
        resp->bytes_read = -1;
        resp->error = IPC_EBADF;
        return;
    }

    if (!fd_entry->node->is_file()) {
        resp->bytes_read = -1;
        resp->error = IPC_EISDIR;
        return;
    }

    auto file = std::static_pointer_cast<RamfsFile>(fd_entry->node);

    // Use offset from request if specified, otherwise use fd offset
    uint64_t offset = (req->offset != static_cast<uint64_t>(-1)) ? req->offset : fd_entry->offset;

    // For small reads (<= 256 bytes), use inline data
    if (req->count <= sizeof(resp->inline_data)) {
        int64_t bytes_read = file->read(resp->inline_data, req->count, offset);
        if (bytes_read >= 0 && req->offset == static_cast<uint64_t>(-1)) {
            fd_entry->offset += bytes_read;
        }
        resp->bytes_read = bytes_read;
        resp->error = (bytes_read >= 0) ? IPC_SUCCESS : IPC_EIO;
    } else {
        // TODO: Handle large reads via shared memory
        resp->bytes_read = -1;
        resp->error = IPC_EINVAL;
    }
}

/**
 * @brief Handle VFS_WRITE request
 */
static void handle_write(const message& request, message& response) {
    const auto* req = reinterpret_cast<const VfsWriteRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<VfsWriteResponse*>(&response.m_u);

    response.m_source = VFS_SERVER_PID;
    response.m_type = VFS_REPLY;

    FdTable* fds = g_state.get_process_fds(req->caller_pid);
    FileDescriptor* fd_entry = fds->get_fd(req->fd);

    if (!fd_entry) {
        resp->bytes_written = -1;
        resp->error = IPC_EBADF;
        return;
    }

    if (!fd_entry->node->is_file()) {
        resp->bytes_written = -1;
        resp->error = IPC_EISDIR;
        return;
    }

    auto file = std::static_pointer_cast<RamfsFile>(fd_entry->node);

    // Use offset from request if specified, otherwise use fd offset
    uint64_t offset = (req->offset != static_cast<uint64_t>(-1)) ? req->offset : fd_entry->offset;

    // For small writes (<= 256 bytes), use inline data
    if (req->count <= sizeof(req->inline_data)) {
        int64_t bytes_written = file->write(req->inline_data, req->count, offset);
        if (bytes_written >= 0 && req->offset == static_cast<uint64_t>(-1)) {
            fd_entry->offset += bytes_written;
        }
        resp->bytes_written = bytes_written;
        resp->error = (bytes_written >= 0) ? IPC_SUCCESS : IPC_EIO;
    } else {
        // TODO: Handle large writes via shared memory
        resp->bytes_written = -1;
        resp->error = IPC_EINVAL;
    }
}

/**
 * @brief Handle VFS_LSEEK request
 */
static void handle_lseek(const message& request, message& response) {
    const auto* req = reinterpret_cast<const VfsLseekRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<VfsLseekResponse*>(&response.m_u);

    response.m_source = VFS_SERVER_PID;
    response.m_type = VFS_REPLY;

    FdTable* fds = g_state.get_process_fds(req->caller_pid);
    FileDescriptor* fd_entry = fds->get_fd(req->fd);

    if (!fd_entry) {
        resp->new_offset = -1;
        resp->error = IPC_EBADF;
        return;
    }

    uint64_t new_offset;
    switch (req->whence) {
        case 0:  // SEEK_SET
            new_offset = req->offset;
            break;
        case 1:  // SEEK_CUR
            new_offset = fd_entry->offset + req->offset;
            break;
        case 2:  // SEEK_END
            new_offset = fd_entry->node->size() + req->offset;
            break;
        default:
            resp->new_offset = -1;
            resp->error = IPC_EINVAL;
            return;
    }

    fd_entry->offset = new_offset;
    resp->new_offset = new_offset;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle VFS_MKDIR request
 */
static void handle_mkdir(const message& request, message& response) {
    const auto* req = reinterpret_cast<const VfsMkdirRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<VfsMkdirResponse*>(&response.m_u);

    response.m_source = VFS_SERVER_PID;
    response.m_type = VFS_REPLY;

    auto [parent, dirname] = resolve_parent(g_state.fs, req->path);
    if (!parent) {
        resp->result = -1;
        resp->error = IPC_ENOENT;
        return;
    }

    auto dir = g_state.fs.create_dir(parent, dirname, req->mode & 0x0FFF);
    resp->result = dir ? 0 : -1;
    resp->error = dir ? IPC_SUCCESS : IPC_EEXIST;
}

/**
 * @brief Handle VFS_RMDIR request
 */
static void handle_rmdir(const message& request, message& response) {
    const auto* req = reinterpret_cast<const VfsRmdirRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<VfsRmdirResponse*>(&response.m_u);

    response.m_source = VFS_SERVER_PID;
    response.m_type = VFS_REPLY;

    auto [parent, dirname] = resolve_parent(g_state.fs, req->path);
    if (!parent) {
        resp->result = -1;
        resp->error = IPC_ENOENT;
        return;
    }

    int result = g_state.fs.remove_node(parent, dirname);
    resp->result = result;
    resp->error = (result == 0) ? IPC_SUCCESS : IPC_ENOTEMPTY;
}

/**
 * @brief Handle VFS_UNLINK request
 */
static void handle_unlink(const message& request, message& response) {
    const auto* req = reinterpret_cast<const VfsUnlinkRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<VfsGenericResponse*>(&response.m_u);

    response.m_source = VFS_SERVER_PID;
    response.m_type = VFS_REPLY;

    auto [parent, filename] = resolve_parent(g_state.fs, req->path);
    if (!parent) {
        resp->result = -1;
        resp->error = IPC_ENOENT;
        return;
    }

    int result = g_state.fs.remove_node(parent, filename);
    resp->result = result;
    resp->error = (result == 0) ? IPC_SUCCESS : IPC_ENOENT;
}

/**
 * @brief Handle VFS_STAT request
 */
static void handle_stat(const message& request, message& response) {
    const auto* req = reinterpret_cast<const VfsStatRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<VfsStatResponse*>(&response.m_u);

    response.m_source = VFS_SERVER_PID;
    response.m_type = VFS_REPLY;

    std::shared_ptr<RamfsNode> node;

    if (req->is_fstat) {
        // fstat: look up by file descriptor
        FdTable* fds = g_state.get_process_fds(req->caller_pid);
        FileDescriptor* fd_entry = fds->get_fd(req->fd);
        if (fd_entry) {
            node = fd_entry->node;
        }
    } else {
        // stat: look up by path
        node = resolve_path(g_state.fs, req->path);
    }

    if (!node) {
        resp->result = -1;
        resp->error = req->is_fstat ? IPC_EBADF : IPC_ENOENT;
        return;
    }

    const auto& meta = node->metadata();
    resp->stat.st_dev = 0;
    resp->stat.st_ino = meta.inode;
    resp->stat.st_mode = meta.mode;
    resp->stat.st_nlink = meta.nlink;
    resp->stat.st_uid = meta.uid;
    resp->stat.st_gid = meta.gid;
    resp->stat.st_rdev = 0;
    resp->stat.st_size = meta.size;
    resp->stat.st_blksize = 4096;
    resp->stat.st_blocks = (meta.size + 511) / 512;
    resp->stat.st_atime = meta.atime;
    resp->stat.st_mtime = meta.mtime;
    resp->stat.st_ctime = meta.ctime;

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
        case VFS_OPEN:
            handle_open(request, response);
            break;
        case VFS_CLOSE:
            handle_close(request, response);
            break;
        case VFS_READ:
            handle_read(request, response);
            break;
        case VFS_WRITE:
            handle_write(request, response);
            break;
        case VFS_LSEEK:
            handle_lseek(request, response);
            break;
        case VFS_MKDIR:
            handle_mkdir(request, response);
            break;
        case VFS_RMDIR:
            handle_rmdir(request, response);
            break;
        case VFS_UNLINK:
            handle_unlink(request, response);
            break;
        case VFS_STAT:
        case VFS_FSTAT:
            handle_stat(request, response);
            break;
        default:
            response.m_source = VFS_SERVER_PID;
            response.m_type = VFS_ERROR;
            auto* resp = reinterpret_cast<VfsGenericResponse*>(&response.m_u);
            resp->result = -1;
            resp->error = IPC_ENOSYS;
            break;
    }
}

/* ========================================================================
 * Server Initialization and Main Loop
 * ======================================================================== */

/**
 * @brief Initialize VFS server
 */
static bool initialize() {
    // Create some initial directories in ramfs
    auto dev_dir = g_state.fs.create_dir(g_state.fs.root(), "dev");
    auto tmp_dir = g_state.fs.create_dir(g_state.fs.root(), "tmp");
    auto etc_dir = g_state.fs.create_dir(g_state.fs.root(), "etc");

    if (!dev_dir || !tmp_dir || !etc_dir) {
        return false;
    }

    return true;
}

/**
 * @brief VFS server main loop
 */
static void server_loop() {
    message request, response;

    while (true) {
        // Receive IPC message from kernel
        int result = lattice::lattice_recv(VFS_SERVER_PID, &request, 0);
        if (result < 0) {
            // TODO: Handle error
            continue;
        }

        // Dispatch request
        std::memset(&response, 0, sizeof(response));
        dispatch_message(request, response);

        // Send response back to kernel (which forwards to caller)
        lattice::lattice_send(VFS_SERVER_PID, request.m_source, &response, 0);
    }
}

} // namespace xinim::vfs_server

/* ========================================================================
 * Entry Point
 * ======================================================================== */

/**
 * @brief VFS server entry point
 */
int main() {
    using namespace xinim::vfs_server;

    if (!initialize()) {
        return 1;
    }

    server_loop();

    return 0;
}
