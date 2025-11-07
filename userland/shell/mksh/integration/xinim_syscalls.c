/**
 * @file xinim_syscalls.c
 * @brief XINIM system call interface for mksh
 * 
 * Provides mksh with XINIM-specific system calls for process management,
 * file I/O, terminal control, and signal handling.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

/* XINIM syscall numbers - must match kernel syscall table */
#define XINIM_SYS_FORK      1
#define XINIM_SYS_EXEC      2
#define XINIM_SYS_EXIT      3
#define XINIM_SYS_WAIT      4
#define XINIM_SYS_OPEN      5
#define XINIM_SYS_READ      6
#define XINIM_SYS_WRITE     7
#define XINIM_SYS_CLOSE     8
#define XINIM_SYS_DUP       9
#define XINIM_SYS_DUP2      10
#define XINIM_SYS_PIPE      11
#define XINIM_SYS_KILL      12
#define XINIM_SYS_SIGNAL    13
#define XINIM_SYS_GETPID    14
#define XINIM_SYS_GETPPID   15
#define XINIM_SYS_GETENV    16
#define XINIM_SYS_SETENV    17

/**
 * @brief Low-level system call interface
 * 
 * @param syscall_num System call number
 * @param arg1..arg5 System call arguments
 * @return System call return value (or -1 on error with errno set)
 */
static inline long xinim_syscall(long syscall_num, long arg1, long arg2,
                                 long arg3, long arg4, long arg5) {
    long ret;
    register long r10 __asm__("r10") = arg4;
    register long r8 __asm__("r8") = arg5;
    
    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "0"(syscall_num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    
    if (ret < 0 && ret >= -4095) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

/* Process Management */

pid_t xinim_fork(void) {
    return (pid_t)xinim_syscall(XINIM_SYS_FORK, 0, 0, 0, 0, 0);
}

int xinim_execve(const char *pathname, char *const argv[], char *const envp[]) {
    return (int)xinim_syscall(XINIM_SYS_EXEC, (long)pathname, 
                              (long)argv, (long)envp, 0, 0);
}

void xinim_exit(int status) {
    xinim_syscall(XINIM_SYS_EXIT, status, 0, 0, 0, 0);
    __builtin_unreachable();
}

pid_t xinim_wait(int *status) {
    return (pid_t)xinim_syscall(XINIM_SYS_WAIT, (long)status, 0, 0, 0, 0);
}

pid_t xinim_waitpid(pid_t pid, int *status, int options) {
    return (pid_t)xinim_syscall(XINIM_SYS_WAIT, pid, (long)status, 
                                options, 0, 0);
}

/* File Operations */

int xinim_open(const char *pathname, int flags, mode_t mode) {
    return (int)xinim_syscall(XINIM_SYS_OPEN, (long)pathname, flags, mode, 0, 0);
}

ssize_t xinim_read(int fd, void *buf, size_t count) {
    return (ssize_t)xinim_syscall(XINIM_SYS_READ, fd, (long)buf, count, 0, 0);
}

ssize_t xinim_write(int fd, const void *buf, size_t count) {
    return (ssize_t)xinim_syscall(XINIM_SYS_WRITE, fd, (long)buf, count, 0, 0);
}

int xinim_close(int fd) {
    return (int)xinim_syscall(XINIM_SYS_CLOSE, fd, 0, 0, 0, 0);
}

int xinim_dup(int oldfd) {
    return (int)xinim_syscall(XINIM_SYS_DUP, oldfd, 0, 0, 0, 0);
}

int xinim_dup2(int oldfd, int newfd) {
    return (int)xinim_syscall(XINIM_SYS_DUP2, oldfd, newfd, 0, 0, 0);
}

int xinim_pipe(int pipefd[2]) {
    return (int)xinim_syscall(XINIM_SYS_PIPE, (long)pipefd, 0, 0, 0, 0);
}

/* Signal Handling */

int xinim_kill(pid_t pid, int sig) {
    return (int)xinim_syscall(XINIM_SYS_KILL, pid, sig, 0, 0, 0);
}

typedef void (*sighandler_t)(int);

sighandler_t xinim_signal(int signum, sighandler_t handler) {
    long ret = xinim_syscall(XINIM_SYS_SIGNAL, signum, (long)handler, 0, 0, 0);
    return (sighandler_t)ret;
}

/* Process Information */

pid_t xinim_getpid(void) {
    return (pid_t)xinim_syscall(XINIM_SYS_GETPID, 0, 0, 0, 0, 0);
}

pid_t xinim_getppid(void) {
    return (pid_t)xinim_syscall(XINIM_SYS_GETPPID, 0, 0, 0, 0, 0);
}

/* Environment */

char *xinim_getenv(const char *name) {
    return (char *)xinim_syscall(XINIM_SYS_GETENV, (long)name, 0, 0, 0, 0);
}

int xinim_setenv(const char *name, const char *value, int overwrite) {
    return (int)xinim_syscall(XINIM_SYS_SETENV, (long)name, 
                              (long)value, overwrite, 0, 0);
}

int xinim_unsetenv(const char *name) {
    return (int)xinim_syscall(XINIM_SYS_SETENV, (long)name, 0, 1, 0, 0);
}

/* Wrapper functions for mksh compatibility */

pid_t fork(void) {
    return xinim_fork();
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    return xinim_execve(pathname, argv, envp);
}

void _exit(int status) {
    xinim_exit(status);
}

pid_t wait(int *status) {
    return xinim_wait(status);
}

pid_t waitpid(pid_t pid, int *status, int options) {
    return xinim_waitpid(pid, status, options);
}
