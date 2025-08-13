/**
 * @file
 * @brief Utility routines used by the memory manager.
 *
 * Provides helper functions for permission checks, memory copying and
 * fatal error handling.
 */

#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/stat.h"
#include "../h/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "mproc.hpp"

#include <cerrno>     // errno
#include <cstddef>    // For std::size_t
#include <cstdint>    // For uintptr_t
#include <cstdio>     // printf
#include <filesystem> // std::filesystem utilities
#include <memory>     // std::unique_ptr

PRIVATE message copy_mess;

namespace {
// Simple RAII wrapper to ensure file descriptors are closed on scope exit.
struct FileDescriptor {
    int fd{-1};
    explicit FileDescriptor(int f) : fd(f) {}
    ~FileDescriptor() noexcept { // Added noexcept
        if (fd >= 0) {
            // In a destructor, we should generally not let exceptions escape.
            // close() can set errno but doesn't throw C++ exceptions.
            // If it could throw, we'd need a try-catch.
            close(fd);
        }
    }
    // Disallow copy to avoid double close.
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
    // Allow move semantics for flexibility.
    FileDescriptor(FileDescriptor &&other) noexcept : fd(other.fd) { other.fd = -1; }
    FileDescriptor &operator=(FileDescriptor &&other) noexcept {
        if (this != &other) {
            if (fd >= 0)
                close(fd);
            fd = other.fd;
            other.fd = -1;
        }
        return *this;
    }
    // Release ownership of the descriptor without closing it
    [[nodiscard]] int release() noexcept {
        int old = fd;
        fd = -1;
        return old;
    }
};
} // namespace

/**
 * @brief Check if the current user may access a file.
 *
 * @param name_buf Path to inspect.
 * @param s_buf Stat structure for resulting data.
 * @param mask Permission bits to verify.
 * @return File descriptor on success or negative errno.
 */
PUBLIC int allowed(const char *name_buf, struct stat *s_buf, int mask) noexcept {
    int raw_fd = open(name_buf, 0);
    if (raw_fd < 0) {
        return -errno; // propagate errno to caller
    }
    FileDescriptor fd(raw_fd);
    if (fstat(fd.fd, s_buf) < 0) {
        panic("allowed: fstat failed", NO_NUM);
    }

    /* Only regular files can be executed. */
    const int mode = s_buf->st_mode & I_TYPE;
    if (mask == X_BIT && mode != I_REGULAR) {
        return (ErrorCode::EACCES);
    }
    /* Even for superuser, at least 1 X bit must be on. */
    if (mp->mp_effuid == SUPER_USER && mask == X_BIT &&
        (s_buf->st_mode & (X_BIT << 6 | X_BIT << 3 | X_BIT)))
        return fd.release();

    /* Right adjust the relevant set of permission bits. */
    int shift = 0;
    if (mp->mp_effuid == s_buf->st_uid)
        shift = 6;
    else if (mp->mp_effgid == s_buf->st_gid)
        shift = 3;

    if (mp->mp_effuid == SUPER_USER && mask != X_BIT)
        return fd.release();
    if (s_buf->st_mode >> shift & mask) /* test the relevant bits */
        return fd.release();            /* permission granted */
    return (ErrorCode::EACCES);         /* permission denied */
}

/**
 * @brief Copy a memory region between processes.
 *
 * Source and destination can be absolute memory or process space.
 * @return OK on success or error code.
 */
PUBLIC int mem_copy(int src_proc, int src_seg, uintptr_t src_vir, int dst_proc, int dst_seg,
                    uintptr_t dst_vir, std::size_t bytes) noexcept {
    /* Transfer a block of data.  The source and destination can each either be a
     * process (including MM) or absolute memory, indicate by setting 'src_proc'
     * or 'dst_proc' to ABS.
     */

    if (bytes == 0) // Changed 0L to 0
        return (OK);
    src_space(copy_mess) = static_cast<char>(src_seg);
    src_proc_nr(copy_mess) = src_proc;
    // src_buffer is a macro for a message field of type char* (e.g., m1p1)
    src_buffer(copy_mess) = reinterpret_cast<char *>(src_vir);

    dst_space(copy_mess) = static_cast<char>(dst_seg);
    dst_proc_nr(copy_mess) = dst_proc;
    // dst_buffer is a macro for a message field of type char*
    dst_buffer(copy_mess) = reinterpret_cast<char *>(dst_vir);

    // copy_bytes is a macro for a message field of type int (e.g., m1i2)
    // This is a potential narrowing conversion if bytes > INT_MAX.
    // This reflects the existing constraint of the message system.
    copy_bytes(copy_mess) = static_cast<int>(bytes);
    sys_copy(&copy_mess);
    return (copy_mess.m_type);
}
/**
 * @brief Stub for unimplemented system calls.
 *
 * Always returns ErrorCode::EINVAL.
 */
PUBLIC int no_sys() noexcept {
    /* A system call number not implemented by MM has been requested. */

    return (ErrorCode::EINVAL);
}

/**
 * @brief Fatal error handler for the memory manager.
 *
 * Prints a message and aborts the system.
 */
PUBLIC void panic(const char *format, int num) noexcept {
    /* Something awful has happened.  Panics are caused when an internal
     * inconsistency is detected, e.g., a programm_ing error or illegal value of a
     * defined constant.
     */

    printf("Memory manager panic: %s ", format);
    if (num != NO_NUM)
        printf("%d", num);
    printf("\n");
    tell_fs(SYNC, 0, 0, 0); /* flush the cache to the disk */
    sys_abort();
}
