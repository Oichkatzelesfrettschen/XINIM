/**
 * @file vfs_server.cpp
 * @brief VFS server IPC message loop (ADR-0009)
 *
 * Replaces the stub in bare_metal_stubs.cpp. Handles VFS IPC messages
 * from the kernel syscall dispatcher and replies with results.
 *
 * Protocol (message struct from include/sys/type.hpp):
 *   m_source = sender PID
 *   m_type   = VFS_* constant (message_types.h)
 *   m_u.m_m1 / m_m2 / m_m3: payload fields
 *
 * VFS_OPEN:   m1p1=path, m1i1=flags, m1i2=mode, m1i3=caller_pid
 * VFS_READ:   m1i1=fd, m1i2=count, m1i3=caller_pid, m1p1=buf
 * VFS_WRITE:  m1i1=fd, m1i2=count, m1i3=caller_pid, m1p1=buf
 * VFS_CLOSE:  m1i1=fd, m1i3=caller_pid
 * VFS_STAT:   m1p1=path, m1i3=caller_pid -> reply m4l1=KStat addr (unused in v1.3.0)
 * VFS_MKDIR:  m1p1=path, m1i1=mode, m1i3=caller_pid
 * VFS_UNLINK: m1p1=path, m1i3=caller_pid
 * VFS_READDIR: m1i1=fd (dir ino), m1p1=buf, m1i2=max_entries, m1i3=caller_pid
 *
 * Reply: m_type = return value (>= 0 = success, < 0 = -errno).
 */

#include "vfs_server.hpp"
#include "inode_table.hpp"
#include "dirent.hpp"
#include "fd_table.hpp"
#include "path_walk.hpp"
#include "mount_table.hpp"
#include "buffer_cache.hpp"
#include "ramfs_ops.hpp"
#include "bare_vfs.hpp"
#include "core_init.hpp"
#include "seed.hpp"

// Include kernel message types
#include "sys/type.hpp"
#include <xinim/ipc/message_types.h>

// Lattice IPC
namespace lattice {
    enum class IpcFlags : uint32_t { NONE = 0 };
    int lattice_send(int src, int dst, const message& msg, IpcFlags flags);
    int lattice_recv(int pid, message* out, IpcFlags flags);
}

// Defined in bare_metal_stubs.cpp inside extern "C"
extern "C" void early_serial_write_char(char c);

inline constexpr int VFS_PID = 2;

// ============================================================================
// Public: vfs_server_init
// ============================================================================

void vfs_server_init() {
    vfs_core_init();
}

bool vfs_server_initialized() {
    return vfs_core_initialized();
}

// ============================================================================
// Public: vfs_server_loop (called from bare_metal_stubs.cpp vfs_server_main)
// ============================================================================

void vfs_server_loop() {
    for (;;) {
        message msg{};
        int rc = lattice::lattice_recv(VFS_PID, &msg, lattice::IpcFlags::NONE);
        if (rc != 0) continue;

        message reply{};
        reply.m_source = VFS_PID;
        int result = -IPC_ENOSYS; // default: not implemented

        switch (msg.m_type) {

        case VFS_OPEN: {
            // m1p1=path, m1i1=flags, m1i2=mode
            const char* path = msg.m_u.m_m1.m1p1;
            int flags        = msg.m_u.m_m1.m1i1;
            // int mode      = msg.m_u.m_m1.m1i2; (mode for O_CREAT, future)
            if (!path) { result = -IPC_EINVAL; break; }

            uint32_t ino = path_walk(path);
            if (ino == 0) { result = -IPC_ENOENT; break; }

            const MountEntry* mnt = mount_resolve(path);
            if (!mnt || !mnt->ops) { result = -IPC_ENOENT; break; }

            int fd = -1;
            result = mnt->ops->open(ino, static_cast<uint32_t>(flags), &fd);
            if (result == 0) result = fd; // return fd on success
            break;
        }

        case VFS_READ: {
            // m1i1=fd, m1i2=count, m1p1=buf
            int fd        = msg.m_u.m_m1.m1i1;
            int count     = msg.m_u.m_m1.m1i2;
            void* buf     = msg.m_u.m_m1.m1p1;
            if (!buf || count <= 0) { result = -IPC_EINVAL; break; }

            FdEntry* fde = fd_get(fd);
            if (!fde) { result = -IPC_EBADF; break; }

            uint32_t ino = fde->ino;
            int64_t  pos = fde->pos;

            // Resolve FsOps for this inode (currently always ramfs)
            result = ramfs_ops.read(ino, buf, static_cast<uint32_t>(count), pos);
            if (result > 0) fde->pos += result;
            break;
        }

        case VFS_WRITE: {
            // m1i1=fd, m1i2=count, m1p1=buf
            int fd        = msg.m_u.m_m1.m1i1;
            int count     = msg.m_u.m_m1.m1i2;
            const void* buf = msg.m_u.m_m1.m1p1;

            // SYS_write to fd=1 (stdout) echoes to serial as before
            if (fd == 1 || fd == 2) {
                // Pass-through to early serial via the caller's buffer
                // This preserves the existing behavior from the stub
                if (buf && count > 0) {
                    const char* s = reinterpret_cast<const char*>(buf);
                    for (int i = 0; i < count; ++i) {
                        // Write via existing early_serial path in bare_metal_stubs.cpp
                        // We use the already-linked serial write_char
                            early_serial_write_char(s[i]);
                    }
                    result = count;
                } else {
                    result = -IPC_EINVAL;
                }
                break;
            }

            if (!buf || count <= 0) { result = -IPC_EINVAL; break; }
            FdEntry* fde = fd_get(fd);
            if (!fde) { result = -IPC_EBADF; break; }

            uint32_t ino = fde->ino;
            int64_t  pos = (fde->flags & O_APPEND) ? static_cast<int64_t>(inode_get(ino) ? inode_get(ino)->size : 0) : fde->pos;

            result = ramfs_ops.write(ino, buf, static_cast<uint32_t>(count), pos);
            if (result > 0) fde->pos = pos + result;
            break;
        }

        case VFS_CLOSE: {
            // m1i1=fd
            int fd = msg.m_u.m_m1.m1i1;
            result = ramfs_ops.close(fd);
            break;
        }

        case VFS_DUP: {
            // m1i1=oldfd: duplicate to lowest available fd
            int oldfd = msg.m_u.m_m1.m1i1;
            FdEntry* old_fde = fd_get(oldfd);
            if (!old_fde) { result = -IPC_EBADF; break; }
            int newfd = fd_allocate(old_fde->ino, old_fde->flags);
            if (newfd < 0) { result = -IPC_EMFILE; break; }
            fd_get(newfd)->pos = old_fde->pos; // inherit file position
            RawInode* ri = inode_get(old_fde->ino);
            if (ri) ri->open_count++;
            result = newfd;
            break;
        }

        case VFS_DUP2: {
            // m1i1=oldfd, m1i2=newfd: duplicate oldfd to exactly newfd
            int oldfd = msg.m_u.m_m1.m1i1;
            int newfd = msg.m_u.m_m1.m1i2;
            if (newfd < 0 || newfd >= static_cast<int>(MAX_FDS)) {
                result = -IPC_EBADF; break;
            }
            FdEntry* old_fde = fd_get(oldfd);
            if (!old_fde) { result = -IPC_EBADF; break; }
            if (oldfd == newfd) { result = newfd; break; } // POSIX: no-op
            // Close newfd if it is currently open (deferred-close semantics)
            if (fd_get(newfd)) ramfs_ops.close(newfd);
            if (fd_allocate_at(newfd, old_fde->ino, old_fde->flags, old_fde->pos) < 0) {
                result = -IPC_EBADF; break;
            }
            RawInode* ri = inode_get(old_fde->ino);
            if (ri) ri->open_count++;
            result = newfd;
            break;
        }

        case VFS_LSEEK: {
            // m2i1=fd, m2i2=whence, m2l1=offset (int64_t)
            // mess_2 keeps int fields (bytes 0-11) and int64 field (bytes 16+) non-overlapping.
            int     fd     = msg.m_u.m_m2.m2i1;
            int     whence = msg.m_u.m_m2.m2i2;
            int64_t offset = msg.m_u.m_m2.m2l1;
            FdEntry* fde   = fd_get(fd);
            if (!fde) { result = -IPC_EBADF; break; }
            RawInode* ino_p = inode_get(fde->ino);
            if (!ino_p) { result = -IPC_EBADF; break; }
            int64_t new_pos;
            if (whence == SEEK_SET) {
                new_pos = offset;
            } else if (whence == SEEK_CUR) {
                new_pos = fde->pos + offset;
            } else if (whence == SEEK_END) {
                new_pos = static_cast<int64_t>(ino_p->size) + offset;
            } else {
                result = -IPC_EINVAL;
                break;
            }
            if (new_pos < 0) { result = -IPC_EINVAL; break; }
            fde->pos = new_pos;
            // Return full 64-bit position via reply m4l1; m_type=0 signals success.
            reply.m_u.m_m4.m4l1 = new_pos;
            result = 0;
            break;
        }

        case VFS_FSTAT: {
            // m1i1=fd
            int fd = msg.m_u.m_m1.m1i1;
            FdEntry* fde = fd_get(fd);
            if (!fde) { result = -IPC_EBADF; break; }
            KStat st{};
            result = ramfs_ops.stat(fde->ino, &st);
            if (result == 0) {
                reply.m_u.m_m4.m4l1 = st.st_size;
                reply.m_u.m_m4.m4l2 = st.st_mtime;
            }
            break;
        }

        case VFS_STAT: {
            // m1p1=path
            const char* path = msg.m_u.m_m1.m1p1;
            if (!path) { result = -IPC_EINVAL; break; }
            uint32_t ino = path_walk(path);
            if (ino == 0) { result = -IPC_ENOENT; break; }
            // In v1.3.0 KStat is returned inline via m4 fields.
            // Minimal: return 0 on success; caller reads size from m4l1.
            KStat st{};
            result = ramfs_ops.stat(ino, &st);
            if (result == 0) {
                reply.m_u.m_m4.m4l1 = st.st_size;
                reply.m_u.m_m4.m4l2 = st.st_mtime;
            }
            break;
        }

        case VFS_MKDIR: {
            // m1p1=path, m1i1=mode
            const char* path = msg.m_u.m_m1.m1p1;
            int mode         = msg.m_u.m_m1.m1i1;
            if (!path) { result = -IPC_EINVAL; break; }

            const char* name = nullptr;
            uint8_t namelen  = 0;
            uint32_t parent  = path_walk_parent(path, &name, &namelen);
            if (parent == 0 || !name) { result = -IPC_ENOENT; break; }
            result = ramfs_ops.mkdir(parent, name, namelen, static_cast<uint16_t>(mode));
            break;
        }

        case VFS_UNLINK: {
            // m1p1=path
            const char* path = msg.m_u.m_m1.m1p1;
            if (!path) { result = -IPC_EINVAL; break; }

            const char* name = nullptr;
            uint8_t namelen  = 0;
            uint32_t parent  = path_walk_parent(path, &name, &namelen);
            if (parent == 0 || !name) { result = -IPC_ENOENT; break; }
            result = ramfs_ops.unlink(parent, name, namelen);
            break;
        }

        case VFS_READDIR: {
            // m1i1=fd, m1p1=buf, m1i2=max_entries
            int fd         = msg.m_u.m_m1.m1i1;
            void* buf      = msg.m_u.m_m1.m1p1;
            int max_ent    = msg.m_u.m_m1.m1i2;
            FdEntry* fde   = fd_get(fd);
            if (!fde) { result = -IPC_EBADF; break; }
            result = ramfs_ops.readdir(fde->ino,
                                       reinterpret_cast<DirEntry*>(buf),
                                       max_ent);
            break;
        }

        default:
            result = -IPC_ENOSYS;
            break;
        }

        reply.m_type = result;
        int caller = msg.m_source;
        if (caller > 0) {
            lattice::lattice_send(VFS_PID, caller, reply, lattice::IpcFlags::NONE);
        }
    }
}
