/* This file contains some useful utility routines used by MM.
 *
 * The entries into the file are:
 *   allowed:	see if an access is permitted
 *   mem_copy:	copy data from somewhere in memory to somewhere else
 *   no_sys:	this routine is called for invalid system call numbers
 *   panic:	MM has run aground of a fatal error and cannot continue
 */

#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/const.h"
#include "../h/error.h"
#include "../h/stat.h"
#include "../h/type.h"
#include "const.hpp"
#include "glo.hpp"
#include "mproc.hpp"

#include <cerrno>     // errno
#include <cstdio>     // printf
#include <filesystem> // std::filesystem utilities
#include <memory>     // std::unique_ptr

PRIVATE message copy_mess;

namespace {
// Simple RAII wrapper to ensure file descriptors are closed on scope exit.
struct FileDescriptor {
    int fd{-1};
    explicit FileDescriptor(int f) : fd(f) {}
    ~FileDescriptor() {
        if (fd >= 0) {
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

/*===========================================================================*
 *				allowed					     *
 *===========================================================================*/
// Determine if the given file is accessible using the specified mask.
// Returns a file descriptor on success or a negative errno value on failure.
PUBLIC int allowed(const char *name_buf, struct stat *s_buf, int mask) {
    // Use RAII to ensure the descriptor is closed when leaving the scope.
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

/*===========================================================================*
 *				mem_copy				     *
 *===========================================================================*/
PUBLIC int mem_copy(int src_proc, int src_seg, long src_vir, int dst_proc, int dst_seg,
                    long dst_vir, long bytes) {
    /* Transfer a block of data.  The source and destination can each either be a
     * process (including MM) or absolute memory, indicate by setting 'src_proc'
     * or 'dst_proc' to ABS.
     */

    if (bytes == 0L)
        return (OK);
    src_space(copy_mess) = static_cast<char>(src_seg);
    src_proc_nr(copy_mess) = src_proc;
    src_buffer(copy_mess) = src_vir;

    dst_space(copy_mess) = static_cast<char>(dst_seg);
    dst_proc_nr(copy_mess) = dst_proc;
    dst_buffer(copy_mess) = dst_vir;

    copy_bytes(copy_mess) = bytes;
    sys_copy(&copy_mess);
    return (copy_mess.m_type);
}
/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys() {
    /* A system call number not implemented by MM has been requested. */

    return (ErrorCode::EINVAL);
}

/*===========================================================================*
 *				panic					     *
 *===========================================================================*/
PUBLIC void panic(const char *format, int num) {
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
