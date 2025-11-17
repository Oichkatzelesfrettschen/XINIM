/**
 * @file vfs_protocol.hpp
 * @brief VFS Server IPC Protocol Structures
 *
 * This header defines the request and response structures used for
 * communication between the kernel syscall dispatcher and the VFS server.
 * All structures are designed to fit within the XINIM message payload.
 *
 * @ingroup ipc
 */

#ifndef XINIM_IPC_VFS_PROTOCOL_HPP
#define XINIM_IPC_VFS_PROTOCOL_HPP

#include <cstdint>
#include <cstring>
#include "message_types.h"
#include "../sys/syscalls.h"

namespace xinim::ipc {

/* ========================================================================
 * File I/O Protocol Structures
 * ======================================================================== */

/**
 * @brief Request to open a file
 * Message type: VFS_OPEN
 */
struct VfsOpenRequest {
    char path[256];          /**< File path (null-terminated) */
    int32_t flags;           /**< Open flags (O_RDONLY, O_WRONLY, O_RDWR, etc.) */
    int32_t mode;            /**< File mode (permissions) if creating */
    int32_t caller_pid;      /**< PID of calling process */

    VfsOpenRequest() : flags(0), mode(0), caller_pid(0) {
        path[0] = '\0';
    }

    VfsOpenRequest(const char* p, int32_t f, int32_t m, int32_t pid)
        : flags(f), mode(m), caller_pid(pid) {
        std::strncpy(path, p, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }
} __attribute__((packed));

/**
 * @brief Response to open request
 * Message type: VFS_REPLY
 */
struct VfsOpenResponse {
    int32_t fd;              /**< File descriptor (>= 0 on success, -1 on error) */
    int32_t error;           /**< Error code (IPC_SUCCESS or IPC_E*) */

    VfsOpenResponse() : fd(-1), error(IPC_ENOSYS) {}
    VfsOpenResponse(int32_t f, int32_t e) : fd(f), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to close a file
 * Message type: VFS_CLOSE
 */
struct VfsCloseRequest {
    int32_t fd;              /**< File descriptor to close */
    int32_t caller_pid;      /**< PID of calling process */

    VfsCloseRequest() : fd(-1), caller_pid(0) {}
    VfsCloseRequest(int32_t f, int32_t pid) : fd(f), caller_pid(pid) {}
} __attribute__((packed));

/**
 * @brief Response to close request
 * Message type: VFS_REPLY
 */
struct VfsCloseResponse {
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    VfsCloseResponse() : result(-1), error(IPC_ENOSYS) {}
    VfsCloseResponse(int32_t r, int32_t e) : result(r), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to read from a file
 * Message type: VFS_READ
 */
struct VfsReadRequest {
    int32_t fd;              /**< File descriptor */
    uint64_t count;          /**< Number of bytes to read */
    uint64_t offset;         /**< File offset (for pread, -1 for current position) */
    int32_t caller_pid;      /**< PID of calling process */

    VfsReadRequest() : fd(-1), count(0), offset(0), caller_pid(0) {}
    VfsReadRequest(int32_t f, uint64_t c, uint64_t o, int32_t pid)
        : fd(f), count(c), offset(o), caller_pid(pid) {}
} __attribute__((packed));

/**
 * @brief Response to read request
 * Message type: VFS_REPLY
 *
 * Note: Data is transferred via shared memory or follow-up messages
 * for large reads. Small reads (<= 256 bytes) can use inline data.
 */
struct VfsReadResponse {
    int64_t bytes_read;      /**< Number of bytes read (>= 0 on success, -1 on error) */
    int32_t error;           /**< Error code */
    uint8_t inline_data[256]; /**< Inline data for small reads */

    VfsReadResponse() : bytes_read(-1), error(IPC_ENOSYS) {
        std::memset(inline_data, 0, sizeof(inline_data));
    }
} __attribute__((packed));

/**
 * @brief Request to write to a file
 * Message type: VFS_WRITE
 */
struct VfsWriteRequest {
    int32_t fd;              /**< File descriptor */
    uint64_t count;          /**< Number of bytes to write */
    uint64_t offset;         /**< File offset (for pwrite, -1 for current position) */
    int32_t caller_pid;      /**< PID of calling process */
    uint8_t inline_data[256]; /**< Inline data for small writes */

    VfsWriteRequest() : fd(-1), count(0), offset(0), caller_pid(0) {
        std::memset(inline_data, 0, sizeof(inline_data));
    }
} __attribute__((packed));

/**
 * @brief Response to write request
 * Message type: VFS_REPLY
 */
struct VfsWriteResponse {
    int64_t bytes_written;   /**< Number of bytes written (>= 0 on success, -1 on error) */
    int32_t error;           /**< Error code */

    VfsWriteResponse() : bytes_written(-1), error(IPC_ENOSYS) {}
    VfsWriteResponse(int64_t b, int32_t e) : bytes_written(b), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to seek within a file
 * Message type: VFS_LSEEK
 */
struct VfsLseekRequest {
    int32_t fd;              /**< File descriptor */
    int64_t offset;          /**< Offset */
    int32_t whence;          /**< SEEK_SET, SEEK_CUR, or SEEK_END */
    int32_t caller_pid;      /**< PID of calling process */

    VfsLseekRequest() : fd(-1), offset(0), whence(0), caller_pid(0) {}
    VfsLseekRequest(int32_t f, int64_t o, int32_t w, int32_t pid)
        : fd(f), offset(o), whence(w), caller_pid(pid) {}
} __attribute__((packed));

/**
 * @brief Response to lseek request
 * Message type: VFS_REPLY
 */
struct VfsLseekResponse {
    int64_t new_offset;      /**< New file offset (>= 0 on success, -1 on error) */
    int32_t error;           /**< Error code */

    VfsLseekResponse() : new_offset(-1), error(IPC_ENOSYS) {}
    VfsLseekResponse(int64_t o, int32_t e) : new_offset(o), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to get file status
 * Message type: VFS_STAT or VFS_FSTAT
 */
struct VfsStatRequest {
    union {
        char path[256];      /**< File path (for stat) */
        int32_t fd;          /**< File descriptor (for fstat) */
    };
    int32_t caller_pid;      /**< PID of calling process */
    bool is_fstat;           /**< true for fstat, false for stat */

    VfsStatRequest() : fd(-1), caller_pid(0), is_fstat(false) {}
} __attribute__((packed));

/**
 * @brief File status information (simplified stat structure)
 */
struct VfsStatInfo {
    uint64_t st_dev;         /**< Device ID */
    uint64_t st_ino;         /**< Inode number */
    uint32_t st_mode;        /**< File mode (type and permissions) */
    uint32_t st_nlink;       /**< Number of hard links */
    uint32_t st_uid;         /**< User ID */
    uint32_t st_gid;         /**< Group ID */
    uint64_t st_rdev;        /**< Device ID (for special files) */
    int64_t st_size;         /**< File size in bytes */
    int64_t st_blksize;      /**< Block size for filesystem I/O */
    int64_t st_blocks;       /**< Number of 512B blocks allocated */
    int64_t st_atime;        /**< Access time (seconds since epoch) */
    int64_t st_mtime;        /**< Modification time */
    int64_t st_ctime;        /**< Status change time */

    VfsStatInfo() : st_dev(0), st_ino(0), st_mode(0), st_nlink(0),
                    st_uid(0), st_gid(0), st_rdev(0), st_size(0),
                    st_blksize(4096), st_blocks(0),
                    st_atime(0), st_mtime(0), st_ctime(0) {}
} __attribute__((packed));

/**
 * @brief Response to stat/fstat request
 * Message type: VFS_REPLY
 */
struct VfsStatResponse {
    VfsStatInfo stat;        /**< File status information */
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    VfsStatResponse() : result(-1), error(IPC_ENOSYS) {}
} __attribute__((packed));

/* ========================================================================
 * Directory Operations Protocol Structures
 * ======================================================================== */

/**
 * @brief Request to create a directory
 * Message type: VFS_MKDIR
 */
struct VfsMkdirRequest {
    char path[256];          /**< Directory path */
    int32_t mode;            /**< Directory permissions */
    int32_t caller_pid;      /**< PID of calling process */

    VfsMkdirRequest() : mode(0), caller_pid(0) {
        path[0] = '\0';
    }

    VfsMkdirRequest(const char* p, int32_t m, int32_t pid)
        : mode(m), caller_pid(pid) {
        std::strncpy(path, p, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }
} __attribute__((packed));

/**
 * @brief Response to mkdir request
 * Message type: VFS_REPLY
 */
struct VfsMkdirResponse {
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    VfsMkdirResponse() : result(-1), error(IPC_ENOSYS) {}
    VfsMkdirResponse(int32_t r, int32_t e) : result(r), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to remove a directory
 * Message type: VFS_RMDIR
 */
struct VfsRmdirRequest {
    char path[256];          /**< Directory path */
    int32_t caller_pid;      /**< PID of calling process */

    VfsRmdirRequest() : caller_pid(0) {
        path[0] = '\0';
    }
} __attribute__((packed));

/**
 * @brief Response to rmdir request
 * Message type: VFS_REPLY
 */
struct VfsRmdirResponse {
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    VfsRmdirResponse() : result(-1), error(IPC_ENOSYS) {}
    VfsRmdirResponse(int32_t r, int32_t e) : result(r), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to change current directory
 * Message type: VFS_CHDIR
 */
struct VfsChdirRequest {
    char path[256];          /**< New current directory path */
    int32_t caller_pid;      /**< PID of calling process */

    VfsChdirRequest() : caller_pid(0) {
        path[0] = '\0';
    }
} __attribute__((packed));

/**
 * @brief Request to get current working directory
 * Message type: VFS_GETCWD
 */
struct VfsGetcwdRequest {
    int32_t caller_pid;      /**< PID of calling process */
    uint64_t bufsize;        /**< Size of buffer for path */

    VfsGetcwdRequest() : caller_pid(0), bufsize(0) {}
} __attribute__((packed));

/**
 * @brief Response to getcwd request
 * Message type: VFS_REPLY
 */
struct VfsGetcwdResponse {
    char path[256];          /**< Current working directory */
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    VfsGetcwdResponse() : result(-1), error(IPC_ENOSYS) {
        path[0] = '\0';
    }
} __attribute__((packed));

/**
 * @brief Request to unlink (remove) a file
 * Message type: VFS_UNLINK
 */
struct VfsUnlinkRequest {
    char path[256];          /**< File path */
    int32_t caller_pid;      /**< PID of calling process */

    VfsUnlinkRequest() : caller_pid(0) {
        path[0] = '\0';
    }
} __attribute__((packed));

/**
 * @brief Request to rename a file
 * Message type: VFS_RENAME
 */
struct VfsRenameRequest {
    char old_path[128];      /**< Old path */
    char new_path[128];      /**< New path */
    int32_t caller_pid;      /**< PID of calling process */

    VfsRenameRequest() : caller_pid(0) {
        old_path[0] = '\0';
        new_path[0] = '\0';
    }
} __attribute__((packed));

/* ========================================================================
 * Generic Response Structure
 * ======================================================================== */

/**
 * @brief Generic VFS response for simple operations
 * Message type: VFS_REPLY
 */
struct VfsGenericResponse {
    int32_t result;          /**< Operation result (0 = success, -1 = error) */
    int32_t error;           /**< Error code (IPC_SUCCESS or IPC_E*) */
    int64_t extra_data;      /**< Optional extra return value */

    VfsGenericResponse() : result(-1), error(IPC_ENOSYS), extra_data(0) {}
    VfsGenericResponse(int32_t r, int32_t e, int64_t x = 0)
        : result(r), error(e), extra_data(x) {}
} __attribute__((packed));

/* ========================================================================
 * Helper Functions
 * ======================================================================== */

/**
 * @brief Check if a message type is a VFS message
 */
inline constexpr bool is_vfs_message(int msg_type) noexcept {
    return msg_type >= 100 && msg_type < 200;
}

/**
 * @brief Get human-readable name for VFS message type
 */
inline const char* vfs_message_name(int msg_type) noexcept {
    switch (msg_type) {
        case VFS_OPEN: return "VFS_OPEN";
        case VFS_CLOSE: return "VFS_CLOSE";
        case VFS_READ: return "VFS_READ";
        case VFS_WRITE: return "VFS_WRITE";
        case VFS_LSEEK: return "VFS_LSEEK";
        case VFS_STAT: return "VFS_STAT";
        case VFS_FSTAT: return "VFS_FSTAT";
        case VFS_MKDIR: return "VFS_MKDIR";
        case VFS_RMDIR: return "VFS_RMDIR";
        case VFS_CHDIR: return "VFS_CHDIR";
        case VFS_GETCWD: return "VFS_GETCWD";
        case VFS_UNLINK: return "VFS_UNLINK";
        case VFS_RENAME: return "VFS_RENAME";
        case VFS_REPLY: return "VFS_REPLY";
        case VFS_ERROR: return "VFS_ERROR";
        default: return "UNKNOWN_VFS_MESSAGE";
    }
}

} // namespace xinim::ipc

#endif /* XINIM_IPC_VFS_PROTOCOL_HPP */
