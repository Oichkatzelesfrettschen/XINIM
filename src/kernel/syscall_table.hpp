/**
 * @file syscall_table.hpp
 * @brief System call dispatch table and definitions
 *
 * Defines syscall numbers and dispatch mechanism for XINIM.
 * Syscalls use the fast syscall/sysret mechanism on x86_64.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_SYSCALL_TABLE_HPP
#define XINIM_KERNEL_SYSCALL_TABLE_HPP

#include <cstdint>

namespace xinim::kernel {

// ============================================================================
// Syscall Numbers (aligned with Linux/POSIX where possible)
// ============================================================================

/**
 * @brief Syscall numbers for XINIM
 *
 * Categories:
 * - 0-99: Process management
 * - 100-199: File I/O
 * - 200-299: Memory management
 * - 300-399: IPC
 * - 400-499: Networking
 * - 500-599: Time/signals
 */
enum class SyscallNumber : uint64_t {
    // Week 8 Phase 4: Minimal syscall set
    READ        = 0,    ///< read(fd, buf, count)
    WRITE       = 1,    ///< write(fd, buf, count)
    OPEN        = 2,    ///< open(path, flags, mode)
    CLOSE       = 3,    ///< close(fd)
    LSEEK       = 8,    ///< lseek(fd, offset, whence)

    GETPID      = 39,   ///< getpid()
    FORK        = 57,   ///< fork()
    EXEC        = 59,   ///< execve(path, argv, envp)
    EXIT        = 60,   ///< exit(int status)
    WAIT4       = 61,   ///< wait4(pid, status, options, rusage)
    GETPPID     = 110,  ///< getppid()

    // For Week 8 Phase 4, we implement:
    // WRITE (1), GETPID (39), EXIT (60)
};

constexpr size_t MAX_SYSCALLS = 512;

// ============================================================================
// Syscall Handler Function Signature
// ============================================================================

/**
 * @brief Syscall handler function signature
 *
 * All syscalls follow this signature. Arguments are passed in registers:
 * - arg1: RDI
 * - arg2: RSI
 * - arg3: RDX
 * - arg4: R10
 * - arg5: R8
 * - arg6: R9
 *
 * @return int64_t - Return value (positive) or error code (negative)
 */
using SyscallHandler = int64_t (*)(uint64_t arg1, uint64_t arg2,
                                     uint64_t arg3, uint64_t arg4,
                                     uint64_t arg5, uint64_t arg6);

// ============================================================================
// Syscall Dispatch Function
// ============================================================================

/**
 * @brief Dispatch syscall to appropriate handler
 *
 * Called from assembly syscall_handler. Validates syscall number and
 * calls the corresponding handler function.
 *
 * @param syscall_num Syscall number (from RAX)
 * @param arg1-arg6 Syscall arguments (from registers)
 * @return int64_t - Return value or error code
 */
extern "C" int64_t syscall_dispatch(uint64_t syscall_num,
                                     uint64_t arg1, uint64_t arg2,
                                     uint64_t arg3, uint64_t arg4,
                                     uint64_t arg5, uint64_t arg6);

// ============================================================================
// Error Codes (negative return values)
// ============================================================================

constexpr int64_t ENOSYS = 38;  ///< Function not implemented
constexpr int64_t EBADF  = 9;   ///< Bad file descriptor
constexpr int64_t EINVAL = 22;  ///< Invalid argument
constexpr int64_t EFAULT = 14;  ///< Bad address

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_SYSCALL_TABLE_HPP */
