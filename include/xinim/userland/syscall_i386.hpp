#pragma once

#include <stdint.h>

#include "xinim/sys/syscalls.h"
#include "xinim/userland/userspace_stat.hpp"

namespace xinim::userland::x86_32 {

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

inline uint32_t mkdir(const char* path, uint32_t mode) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_mkdir),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)),
        mode);
}

inline uint32_t rmdir(const char* path) noexcept {
    return syscall1(
        static_cast<uint32_t>(SYS_rmdir),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)));
}

inline uint32_t rename(const char* old_path, const char* new_path) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_rename),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(old_path)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(new_path)));
}

inline uint32_t unlink(const char* path) noexcept {
    return syscall1(
        static_cast<uint32_t>(SYS_unlink),
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

inline uint32_t lseek(int fd, int32_t offset, int whence) noexcept {
    return syscall3(
        static_cast<uint32_t>(SYS_lseek),
        static_cast<uint32_t>(fd),
        static_cast<uint32_t>(offset),
        static_cast<uint32_t>(whence));
}

inline uint32_t stat(const char* path, xinim::userland::UserspaceStat* buffer) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_stat),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buffer)));
}

inline uint32_t fstat(int fd, xinim::userland::UserspaceStat* buffer) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_fstat),
        static_cast<uint32_t>(fd),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buffer)));
}

inline uint32_t pipe(int pipefd[2]) noexcept {
    return syscall1(
        static_cast<uint32_t>(SYS_pipe),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pipefd)));
}

inline uint32_t kill(int pid, int sig) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_kill),
        static_cast<uint32_t>(pid),
        static_cast<uint32_t>(sig));
}

inline uint32_t getuid() noexcept {
    return syscall0(static_cast<uint32_t>(SYS_getuid));
}

inline uint32_t geteuid() noexcept {
    return syscall0(static_cast<uint32_t>(SYS_geteuid));
}

inline uint32_t getgid() noexcept {
    return syscall0(static_cast<uint32_t>(SYS_getgid));
}

inline uint32_t getegid() noexcept {
    return syscall0(static_cast<uint32_t>(SYS_getegid));
}

inline uint32_t getdents(int fd, void* dirp, uint32_t count) noexcept {
    return syscall3(
        static_cast<uint32_t>(SYS_getdents),
        static_cast<uint32_t>(fd),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dirp)),
        count);
}

inline uint32_t nanosleep(const void* req, void* rem) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_nanosleep),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(req)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(rem)));
}

inline uint32_t symlink(const char* target, const char* linkpath) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_symlink),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(linkpath)));
}

inline uint32_t readlink(const char* path, char* buf, uint32_t bufsiz) noexcept {
    return syscall3(
        static_cast<uint32_t>(SYS_readlink),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buf)),
        bufsiz);
}

inline uint32_t chmod(const char* path, uint32_t mode) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_chmod),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)),
        mode);
}

inline uint32_t chown(const char* path, uint32_t owner, uint32_t group) noexcept {
    return syscall3(
        static_cast<uint32_t>(SYS_chown),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)),
        owner,
        group);
}

inline uint32_t uname(void* buf) noexcept {
    return syscall1(
        static_cast<uint32_t>(SYS_uname),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buf)));
}

inline uint32_t dup2(int oldfd, int newfd) noexcept {
    return syscall2(
        static_cast<uint32_t>(SYS_dup2),
        static_cast<uint32_t>(oldfd),
        static_cast<uint32_t>(newfd));
}

inline uint32_t ioctl(int fd, uint32_t request, uint32_t arg) noexcept {
    return syscall3(
        static_cast<uint32_t>(SYS_ioctl),
        static_cast<uint32_t>(fd),
        request,
        arg);
}

inline uint32_t brk(uint32_t addr) noexcept {
    return syscall1(static_cast<uint32_t>(SYS_brk), addr);
}

} // namespace xinim::userland::x86_32
