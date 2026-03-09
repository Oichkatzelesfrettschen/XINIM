#pragma once

#include <stdint.h>
#include "xinim/sys/syscalls.h"

namespace xinim::userland::x86_64 {

inline uint64_t syscall0(uint64_t number) noexcept {
    uint64_t result = 0;
    asm volatile(
        "syscall"
        : "=a"(result)
        : "a"(number)
        : "rcx", "r11", "memory", "cc");
    return result;
}

inline uint64_t syscall1(uint64_t number, uint64_t arg0) noexcept {
    uint64_t result = 0;
    asm volatile(
        "syscall"
        : "=a"(result)
        : "a"(number), "D"(arg0)
        : "rcx", "r11", "memory", "cc");
    return result;
}

inline uint64_t syscall2(uint64_t number, uint64_t arg0, uint64_t arg1) noexcept {
    uint64_t result = 0;
    asm volatile(
        "syscall"
        : "=a"(result)
        : "a"(number), "D"(arg0), "S"(arg1)
        : "rcx", "r11", "memory", "cc");
    return result;
}

inline uint64_t syscall3(uint64_t number,
                         uint64_t arg0,
                         uint64_t arg1,
                         uint64_t arg2) noexcept {
    uint64_t result = 0;
    asm volatile(
        "syscall"
        : "=a"(result)
        : "a"(number), "D"(arg0), "S"(arg1), "d"(arg2)
        : "rcx", "r11", "memory", "cc");
    return result;
}

inline uint64_t syscall4(uint64_t number,
                         uint64_t arg0,
                         uint64_t arg1,
                         uint64_t arg2,
                         uint64_t arg3) noexcept {
    register uint64_t r10 __asm__("r10") = arg3;
    uint64_t result = 0;
    asm volatile(
        "syscall"
        : "=a"(result)
        : "a"(number), "D"(arg0), "S"(arg1), "d"(arg2), "r"(r10)
        : "rcx", "r11", "memory", "cc");
    return result;
}

inline uint64_t syscall5(uint64_t number,
                         uint64_t arg0,
                         uint64_t arg1,
                         uint64_t arg2,
                         uint64_t arg3,
                         uint64_t arg4) noexcept {
    register uint64_t r10 __asm__("r10") = arg3;
    register uint64_t r8 __asm__("r8") = arg4;
    uint64_t result = 0;
    asm volatile(
        "syscall"
        : "=a"(result)
        : "a"(number), "D"(arg0), "S"(arg1), "d"(arg2), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory", "cc");
    return result;
}

inline uint64_t syscall6(uint64_t number,
                         uint64_t arg0,
                         uint64_t arg1,
                         uint64_t arg2,
                         uint64_t arg3,
                         uint64_t arg4,
                         uint64_t arg5) noexcept {
    register uint64_t r10 __asm__("r10") = arg3;
    register uint64_t r8 __asm__("r8") = arg4;
    register uint64_t r9 __asm__("r9") = arg5;
    uint64_t result = 0;
    asm volatile(
        "syscall"
        : "=a"(result)
        : "a"(number), "D"(arg0), "S"(arg1), "d"(arg2), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory", "cc");
    return result;
}

inline uint64_t read(uint64_t fd, void* buffer, uint64_t count) noexcept {
    return syscall3(static_cast<uint64_t>(SYS_read), fd,
                    reinterpret_cast<uint64_t>(buffer), count);
}

inline uint64_t write(uint64_t fd, const void* buffer, uint64_t count) noexcept {
    return syscall3(static_cast<uint64_t>(SYS_write), fd,
                    reinterpret_cast<uint64_t>(buffer), count);
}

[[noreturn]] inline void exit(int status) noexcept {
    (void)syscall1(static_cast<uint64_t>(SYS_exit), static_cast<uint64_t>(status));
    for (;;) {
        asm volatile("hlt");
    }
}

inline uint64_t getpid() noexcept {
    return syscall0(static_cast<uint64_t>(SYS_getpid));
}

inline uint64_t open(const char* pathname, uint64_t flags, uint64_t mode) noexcept {
    return syscall3(static_cast<uint64_t>(SYS_open),
                    reinterpret_cast<uint64_t>(pathname),
                    flags, mode);
}

inline uint64_t close(int fd) noexcept {
    return syscall1(static_cast<uint64_t>(SYS_close), static_cast<uint64_t>(fd));
}

inline uint64_t access(const char* path, int mode) noexcept {
    return syscall2(static_cast<uint64_t>(SYS_access),
                    reinterpret_cast<uint64_t>(path),
                    static_cast<uint64_t>(mode));
}

inline uint64_t chdir(const char* path) noexcept {
    return syscall1(static_cast<uint64_t>(SYS_chdir), reinterpret_cast<uint64_t>(path));
}

inline uint64_t getcwd(char* buffer, uint64_t size) noexcept {
    return syscall2(static_cast<uint64_t>(SYS_getcwd),
                    reinterpret_cast<uint64_t>(buffer), size);
}

inline uint64_t fork() noexcept {
    return syscall0(static_cast<uint64_t>(SYS_fork));
}

inline uint64_t execve(const char* path, char* const* argv, char* const* envp) noexcept {
    return syscall3(static_cast<uint64_t>(SYS_execve),
                    reinterpret_cast<uint64_t>(path),
                    reinterpret_cast<uint64_t>(argv),
                    reinterpret_cast<uint64_t>(envp));
}

inline uint64_t wait4(int pid, int* status, int options, void* rusage) noexcept {
    return syscall4(static_cast<uint64_t>(SYS_wait4),
                    static_cast<uint64_t>(pid),
                    reinterpret_cast<uint64_t>(status),
                    static_cast<uint64_t>(options),
                    reinterpret_cast<uint64_t>(rusage));
}

inline uint64_t lseek(int fd, int64_t offset, int whence) noexcept {
    return syscall3(static_cast<uint64_t>(SYS_lseek),
                    static_cast<uint64_t>(fd),
                    static_cast<uint64_t>(offset),
                    static_cast<uint64_t>(whence));
}

} // namespace xinim::userland::x86_64
