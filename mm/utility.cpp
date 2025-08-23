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

#include <array>      // std::array
#include <cerrno>     // errno
#include <cstddef>    // For std::size_t
#include <cstdint>    // For uintptr_t
#include <cstdio>     // printf
#include <filesystem> // std::filesystem utilities
#include <memory>     // std::unique_ptr
#include <optional>   // std::optional
#include <ranges>     // std::ranges algorithms
#include <utility>    // std::pair

PRIVATE message copy_mess;

namespace {
/**
 * @brief RAII wrapper ensuring a file descriptor is closed on scope exit.
 *
 * The wrapper is move-only to prevent double closes and provides a
 * @c release() helper to transfer ownership.
 */
struct FileDescriptor {
    /// Wrapped file descriptor value.
    int fd{-1};
    /// Construct from a raw descriptor.
    explicit FileDescriptor(int f) : fd(f) {}
    /// Close the descriptor if it is valid.
    ~FileDescriptor() noexcept {
        if (fd >= 0) {
            // In a destructor, we should generally not let exceptions escape.
            // close() can set errno but doesn't throw C++ exceptions.
            // If it could throw, we'd need a try-catch.
            close(fd);
        }
    }
    /// Deleted copy constructor.
    FileDescriptor(const FileDescriptor &) = delete;
    /// Deleted copy assignment operator.
    FileDescriptor &operator=(const FileDescriptor &) = delete;
    /// Move constructor.
    FileDescriptor(FileDescriptor &&other) noexcept : fd(other.fd) { other.fd = -1; }
    /// Move assignment operator.
    FileDescriptor &operator=(FileDescriptor &&other) noexcept {
        if (this != &other) {
            if (fd >= 0)
                close(fd);
            fd = other.fd;
            other.fd = -1;
        }
        return *this;
    }
    /// Release ownership of the descriptor without closing it.
    [[nodiscard]] int release() noexcept {
        int old = fd;
        fd = -1;
        return old;
    }
};

/**
 * @brief Determine if any execute bit is present in @p mode.
 *
 * Utilizes @c std::ranges::any_of for clarity.
 */
[[nodiscard]] bool has_exec_bits(mode_t mode) noexcept {
    constexpr std::array<mode_t, 3> exec_bits{X_BIT << 6, X_BIT << 3, X_BIT};
    return std::ranges::any_of(exec_bits, [mode](mode_t bit) { return (mode & bit) != 0; });
}

/**
 * @brief Compute the permission shift based on file ownership.
 *
 * Uses @c std::ranges::find_if to locate the applicable rule and returns
 * the shift amount as an @c std::optional.
 *
 * @param effuid Effective user ID requesting access.
 * @param effgid Effective group ID requesting access.
 * @param st     Stat structure describing the file.
 * @return 6 for owner, 3 for group or an empty optional if neither match.
 */
[[nodiscard]] std::optional<int> ownership_shift(uid_t effuid, gid_t effgid,
                                                 const struct stat *st) noexcept {
    const std::array<std::pair<bool, int>, 2> rules{
        {{effuid == st->st_uid, 6}, {effgid == st->st_gid, 3}}};
    if (auto it = std::ranges::find_if(rules, [](const auto &rule) { return rule.first; });
        it != rules.end()) {
        return it->second;
    }
    return std::nullopt;
}
} // namespace

/**
 * @brief Check if the current user may access a file.
 *
 * @param name_buf Path to inspect.
 * @param s_buf Stat structure for resulting data.
 * @param mask Permission bits to verify.
 * @return File descriptor on success or negative errno.
 */
[[nodiscard]] PUBLIC int allowed(const char *name_buf, struct stat *s_buf, int mask) noexcept {
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
    if (mp->mp_effuid == SUPER_USER && mask == X_BIT && has_exec_bits(s_buf->st_mode))
        return fd.release();

    /* Right adjust the relevant set of permission bits. */
    const int shift = ownership_shift(mp->mp_effuid, mp->mp_effgid, s_buf).value_or(0);

    if (mp->mp_effuid == SUPER_USER && mask != X_BIT)
        return fd.release();
    if ((s_buf->st_mode >> shift) & mask) /* test the relevant bits */
        return fd.release();              /* permission granted */
    return (ErrorCode::EACCES);           /* permission denied */
}

/**
 * @brief Copy a memory region between processes.
 *
 * Source and destination can be absolute memory or process space.
 * @return OK on success or error code.
 */
[[nodiscard]] PUBLIC int mem_copy(int src_proc, int src_seg, uintptr_t src_vir, int dst_proc,
                                  int dst_seg, uintptr_t dst_vir, std::size_t bytes) noexcept {
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
[[nodiscard]] PUBLIC int no_sys() noexcept {
    /* A system call number not implemented by MM has been requested. */

    return (ErrorCode::EINVAL);
}

/**
 * @brief Fatal error handler for the memory manager.
 *
 * Prints a message and aborts the system.
 */
[[noreturn]] PUBLIC void panic(const char *format, int num) noexcept {
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
