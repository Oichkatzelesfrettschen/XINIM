#pragma once

#include <stdint.h>

#include "xinim/sys/syscalls.h"

namespace xinim::userland::i386 {

inline uint32_t syscall0(uint32_t number) noexcept {
    uint32_t result = 0U;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(number)
        : "memory", "cc");
    return result;
}

inline uint32_t syscall1(uint32_t number, uint32_t arg0) noexcept {
    uint32_t result = 0U;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(arg0)
        : "memory", "cc");
    return result;
}

inline uint32_t syscall2(uint32_t number, uint32_t arg0, uint32_t arg1) noexcept {
    uint32_t result = 0U;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(arg0), "c"(arg1)
        : "memory", "cc");
    return result;
}

inline uint32_t syscall3(uint32_t number, uint32_t arg0, uint32_t arg1, uint32_t arg2) noexcept {
    uint32_t result = 0U;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(arg0), "c"(arg1), "d"(arg2)
        : "memory", "cc");
    return result;
}

inline uint32_t syscall4(
    uint32_t number,
    uint32_t arg0,
    uint32_t arg1,
    uint32_t arg2,
    uint32_t arg3) noexcept {
    uint32_t result = 0U;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(arg0), "c"(arg1), "d"(arg2), "S"(arg3)
        : "memory", "cc");
    return result;
}

inline uint32_t syscall5(
    uint32_t number,
    uint32_t arg0,
    uint32_t arg1,
    uint32_t arg2,
    uint32_t arg3,
    uint32_t arg4) noexcept {
    uint32_t result = 0U;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(arg0), "c"(arg1), "d"(arg2), "S"(arg3), "D"(arg4)
        : "memory", "cc");
    return result;
}

inline uint32_t write(int fd, const void* buffer, uint32_t count) noexcept {
    return syscall3(
        static_cast<uint32_t>(SYS_write),
        static_cast<uint32_t>(fd),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buffer)),
        count);
}

inline uint32_t read(int fd, void* buffer, uint32_t count) noexcept {
    return syscall3(
        static_cast<uint32_t>(SYS_read),
        static_cast<uint32_t>(fd),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buffer)),
        count);
}

[[noreturn]] inline void exit(int status) noexcept {
    static_cast<void>(syscall1(static_cast<uint32_t>(SYS_exit), static_cast<uint32_t>(status)));
    for (;;) {
        asm volatile("hlt");
    }
}

inline uint32_t getpid() noexcept {
    return syscall0(static_cast<uint32_t>(SYS_getpid));
}

inline uint32_t access(const char* path, int mode) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_access),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)),
        static_cast<uint32_t>(mode));
}

inline uint32_t open(const char* path, uint32_t flags = 0U, uint32_t mode = 0U) noexcept {
    return syscall3(
        static_cast<uint32_t>(SYS_open),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)),
        flags,
        mode);
}

inline uint32_t close(int fd) noexcept {
    return syscall1(static_cast<uint32_t>(SYS_close), static_cast<uint32_t>(fd));
}

inline uint32_t chdir(const char* path) noexcept {
    return syscall1(
        static_cast<uint32_t>(SYS_chdir),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)));
}

inline uint32_t getcwd(char* buffer, uint32_t size) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_getcwd),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buffer)),
        size);
}

inline uint32_t fork() noexcept {
    return syscall0(static_cast<uint32_t>(SYS_fork));
}

inline uint32_t execve(const char* path, char* const* argv, char* const* envp) noexcept {
    return syscall3(
        static_cast<uint32_t>(SYS_execve),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(argv)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(envp)));
}

inline uint32_t wait4(int pid, int* status, int options, void* rusage) noexcept {
    return syscall4(
        static_cast<uint32_t>(SYS_wait4),
        static_cast<uint32_t>(pid),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(status)),
        static_cast<uint32_t>(options),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(rusage)));
}

} // namespace xinim::userland::i386
