/**
 * @file syscall.h
 * @brief Userspace syscall wrappers for XINIM
 *
 * Provides C function wrappers around x86_64 syscall instruction.
 * These functions can be called from Ring 3 userspace code.
 *
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_SYSCALL_H
#define XINIM_SYSCALL_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Syscall Numbers (must match kernel syscall_table.hpp)
// ============================================================================

#define SYS_read    0
#define SYS_write   1
#define SYS_open    2
#define SYS_close   3
#define SYS_lseek   8
#define SYS_getpid  39
#define SYS_fork    57
#define SYS_exec    59
#define SYS_exit    60
#define SYS_wait4   61
#define SYS_getppid 110

// ============================================================================
// Generic Syscall Functions
// ============================================================================

/**
 * @brief Generic syscall with 0 arguments
 */
static inline int64_t syscall0(uint64_t num) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Generic syscall with 1 argument
 */
static inline int64_t syscall1(uint64_t num, uint64_t arg1) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Generic syscall with 2 arguments
 */
static inline int64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Generic syscall with 3 arguments
 */
static inline int64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Generic syscall with 4 arguments
 */
static inline int64_t syscall4(uint64_t num, uint64_t arg1, uint64_t arg2,
                                uint64_t arg3, uint64_t arg4) {
    int64_t ret;
    register uint64_t r10 __asm__("r10") = arg4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Generic syscall with 5 arguments
 */
static inline int64_t syscall5(uint64_t num, uint64_t arg1, uint64_t arg2,
                                uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    int64_t ret;
    register uint64_t r10 __asm__("r10") = arg4;
    register uint64_t r8  __asm__("r8")  = arg5;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Generic syscall with 6 arguments
 */
static inline int64_t syscall6(uint64_t num, uint64_t arg1, uint64_t arg2,
                                uint64_t arg3, uint64_t arg4, uint64_t arg5,
                                uint64_t arg6) {
    int64_t ret;
    register uint64_t r10 __asm__("r10") = arg4;
    register uint64_t r8  __asm__("r8")  = arg5;
    register uint64_t r9  __asm__("r9")  = arg6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// ============================================================================
// POSIX Function Wrappers
// ============================================================================

/**
 * @brief Write to file descriptor
 */
static inline ssize_t write(int fd, const void* buf, size_t count) {
    return (ssize_t)syscall3(SYS_write, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

/**
 * @brief Get process ID
 */
static inline pid_t getpid(void) {
    return (pid_t)syscall0(SYS_getpid);
}

/**
 * @brief Terminate calling process
 */
static inline void _exit(int status) __attribute__((noreturn));
static inline void _exit(int status) {
    syscall1(SYS_exit, (uint64_t)status);
    __builtin_unreachable();
}

#ifdef __cplusplus
}
#endif

#endif /* XINIM_SYSCALL_H */
