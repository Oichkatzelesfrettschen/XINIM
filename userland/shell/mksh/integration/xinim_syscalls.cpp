#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/**
 * @file xinim_syscalls.cpp
 * @brief C++23 implementation of the mksh-facing XINIM syscall adapter.
 *
 * The exported functions retain C linkage because mksh and libc consume a C
 * ABI. XINIM-owned implementation logic remains C++23 behind that boundary.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xinim/sys/syscalls.h>

namespace {

    /** Invoke the x86_64 syscall instruction using the System V syscall ABI. */
    [[nodiscard]] long invoke_system_call(long syscall_number, long first_argument,
                                          long second_argument, long third_argument,
                                          long fourth_argument, long fifth_argument) noexcept {
        long result = 0;

        asm volatile("mov %[fourth_argument], %%r10\n\t"
                     "mov %[fifth_argument], %%r8\n\t"
                     "syscall"
                     : "=a"(result)
                     : "0"(syscall_number), "D"(first_argument), "S"(second_argument),
                       "d"(third_argument), [fourth_argument] "r"(fourth_argument),
                       [fifth_argument] "r"(fifth_argument)
                     : "r8", "r10", "rcx", "r11", "memory");

        if (result < 0 && result >= -4095) {
            errno = static_cast<int>(-result);
            return -1;
        }
        return result;
    }

    using SignalHandler = void (*)(int);

} // namespace

extern "C" {

pid_t xinim_fork() noexcept {
    return static_cast<pid_t>(invoke_system_call(SYS_fork, 0, 0, 0, 0, 0));
}

int xinim_execve(const char *pathname, char *const arguments[],
                 char *const environment[]) noexcept {
    return static_cast<int>(invoke_system_call(SYS_execve, reinterpret_cast<long>(pathname),
                                               reinterpret_cast<long>(arguments),
                                               reinterpret_cast<long>(environment), 0, 0));
}

[[noreturn]] void xinim_exit(int status) noexcept {
    static_cast<void>(invoke_system_call(SYS_exit, status, 0, 0, 0, 0));
    __builtin_unreachable();
}

pid_t xinim_wait(int *status) noexcept {
    return static_cast<pid_t>(
        invoke_system_call(SYS_wait4, -1, reinterpret_cast<long>(status), 0, 0, 0));
}

pid_t xinim_waitpid(pid_t process_id, int *status, int options) noexcept {
    return static_cast<pid_t>(
        invoke_system_call(SYS_wait4, process_id, reinterpret_cast<long>(status), options, 0, 0));
}

int xinim_open(const char *pathname, int flags, mode_t mode) noexcept {
    return static_cast<int>(
        invoke_system_call(SYS_open, reinterpret_cast<long>(pathname), flags, mode, 0, 0));
}

ssize_t xinim_read(int descriptor, void *buffer, size_t count) noexcept {
    return static_cast<ssize_t>(invoke_system_call(
        SYS_read, descriptor, reinterpret_cast<long>(buffer), static_cast<long>(count), 0, 0));
}

ssize_t xinim_write(int descriptor, const void *buffer, size_t count) noexcept {
    return static_cast<ssize_t>(invoke_system_call(
        SYS_write, descriptor, reinterpret_cast<long>(buffer), static_cast<long>(count), 0, 0));
}

int xinim_close(int descriptor) noexcept {
    return static_cast<int>(invoke_system_call(SYS_close, descriptor, 0, 0, 0, 0));
}

int xinim_dup(int old_descriptor) noexcept {
    return static_cast<int>(invoke_system_call(SYS_dup, old_descriptor, 0, 0, 0, 0));
}

int xinim_dup2(int old_descriptor, int new_descriptor) noexcept {
    return static_cast<int>(invoke_system_call(SYS_dup2, old_descriptor, new_descriptor, 0, 0, 0));
}

int xinim_pipe(int pipe_descriptors[2]) noexcept {
    return static_cast<int>(
        invoke_system_call(SYS_pipe, reinterpret_cast<long>(pipe_descriptors), 0, 0, 0, 0));
}

int xinim_kill(pid_t process_id, int signal_number) noexcept {
    return static_cast<int>(invoke_system_call(SYS_kill, process_id, signal_number, 0, 0, 0));
}

SignalHandler xinim_signal(int signal_number, SignalHandler handler) noexcept {
    const long result =
        invoke_system_call(SYS_signal, signal_number, reinterpret_cast<long>(handler), 0, 0, 0);
    return reinterpret_cast<SignalHandler>(result);
}

pid_t xinim_getpid() noexcept {
    return static_cast<pid_t>(invoke_system_call(SYS_getpid, 0, 0, 0, 0, 0));
}

pid_t xinim_getppid() noexcept {
    return static_cast<pid_t>(invoke_system_call(SYS_getppid, 0, 0, 0, 0, 0));
}

char *xinim_getenv(const char *name) noexcept {
    return getenv(name);
}

int xinim_setenv(const char *name, const char *value, int overwrite) noexcept {
    return setenv(name, value, overwrite);
}

int xinim_unsetenv(const char *name) noexcept {
    return unsetenv(name);
}

pid_t fork() {
    return xinim_fork();
}

int execve(const char *pathname, char *const arguments[], char *const environment[]) {
    return xinim_execve(pathname, arguments, environment);
}

void _exit(int status) {
    xinim_exit(status);
}

pid_t wait(int *status) {
    return xinim_wait(status);
}

pid_t waitpid(pid_t process_id, int *status, int options) {
    return xinim_waitpid(process_id, status, options);
}

} // extern "C"
