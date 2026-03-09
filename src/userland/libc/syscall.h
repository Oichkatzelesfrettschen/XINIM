/**
 * @file syscall.h
 * @brief Legacy C syscall wrappers aligned to the active XINIM syscall ABI.
 *
 * This header exists only for older experimental userland code paths that still
 * include `src/userland/libc/syscall.h`. New i486 userland should prefer
 * `include/xinim/userland/syscall_i386.hpp`.
 */

#ifndef XINIM_USERLAND_LIBC_SYSCALL_H
#define XINIM_USERLAND_LIBC_SYSCALL_H

#include <stdint.h>
#include <sys/types.h>

#include "../../../include/xinim/sys/syscalls.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__i386__)

static inline int32_t syscall0(uint32_t num) {
    int32_t ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num)
        : "memory", "cc");
    return ret;
}

static inline int32_t syscall1(uint32_t num, uint32_t arg1) {
    int32_t ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1)
        : "memory", "cc");
    return ret;
}

static inline int32_t syscall2(uint32_t num, uint32_t arg1, uint32_t arg2) {
    int32_t ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2)
        : "memory", "cc");
    return ret;
}

static inline int32_t syscall3(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    int32_t ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory", "cc");
    return ret;
}

#elif defined(__x86_64__)

static inline int64_t syscall0(uint64_t num) {
    int64_t ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory");
    return ret;
}

static inline int64_t syscall1(uint64_t num, uint64_t arg1) {
    int64_t ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory");
    return ret;
}

static inline int64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    int64_t ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory");
    return ret;
}

static inline int64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    int64_t ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory");
    return ret;
}

#else
#error "Unsupported architecture for XINIM syscall wrappers"
#endif

static inline ssize_t write(int fd, const void* buf, size_t count) {
#if defined(__i386__)
    return (ssize_t)syscall3((uint32_t)SYS_write,
                             (uint32_t)fd,
                             (uint32_t)(uintptr_t)buf,
                             (uint32_t)count);
#else
    return (ssize_t)syscall3((uint64_t)SYS_write,
                             (uint64_t)fd,
                             (uint64_t)(uintptr_t)buf,
                             (uint64_t)count);
#endif
}

static inline ssize_t read(int fd, void* buf, size_t count) {
#if defined(__i386__)
    return (ssize_t)syscall3((uint32_t)SYS_read,
                             (uint32_t)fd,
                             (uint32_t)(uintptr_t)buf,
                             (uint32_t)count);
#else
    return (ssize_t)syscall3((uint64_t)SYS_read,
                             (uint64_t)fd,
                             (uint64_t)(uintptr_t)buf,
                             (uint64_t)count);
#endif
}

static inline pid_t getpid(void) {
#if defined(__i386__)
    return (pid_t)syscall0((uint32_t)SYS_getpid);
#else
    return (pid_t)syscall0((uint64_t)SYS_getpid);
#endif
}

static inline pid_t getppid(void) {
#if defined(__i386__)
    return (pid_t)syscall0((uint32_t)SYS_getppid);
#else
    return (pid_t)syscall0((uint64_t)SYS_getppid);
#endif
}

static inline int access(const char* path, int mode) {
#if defined(__i386__)
    return (int)syscall2((uint32_t)SYS_access,
                         (uint32_t)(uintptr_t)path,
                         (uint32_t)mode);
#else
    return (int)syscall2((uint64_t)SYS_access,
                         (uint64_t)(uintptr_t)path,
                         (uint64_t)mode);
#endif
}

static inline void _exit(int status) __attribute__((noreturn));
static inline void _exit(int status) {
#if defined(__i386__)
    (void)syscall1((uint32_t)SYS_exit, (uint32_t)status);
#else
    (void)syscall1((uint64_t)SYS_exit, (uint64_t)status);
#endif
    __builtin_unreachable();
}

#ifdef __cplusplus
}
#endif

#endif /* XINIM_USERLAND_LIBC_SYSCALL_H */
