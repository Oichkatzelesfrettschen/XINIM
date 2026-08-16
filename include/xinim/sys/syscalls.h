#pragma once
#include <stdint.h>
#include <xinim/abi/native_syscall_numbers.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief XINIM system call numbers.
 *
 * These map to the XINIM microkernel syscall interface, NOT Linux syscalls.
 * dietlibc will be modified to call these syscalls instead of Linux ones.
 */
enum xinim_syscall_no {
    // Debug/Legacy (keep for compatibility)
    SYS_debug_write = 1,
    SYS_monotonic_ns = 2,

    // File I/O (POSIX.1-2017)
    SYS_open = 3,
    SYS_close = 4,
    SYS_read = 5,
    SYS_write = 6,
    SYS_lseek = 7,
    SYS_stat = 8,
    SYS_fstat = 9,
    SYS_access = 10,
    SYS_dup = 11,
    SYS_dup2 = 12,
    SYS_pipe = 13,
    SYS_ioctl = 14,
    SYS_fcntl = 15,

    // Directory operations
    SYS_mkdir = 16,
    SYS_rmdir = 17,
    SYS_chdir = 18,
    SYS_getcwd = 19,
    SYS_link = 20,
    SYS_unlink = 21,
    SYS_rename = 22,
    SYS_chmod = 23,
    SYS_chown = 24,

    // Process management
    SYS_exit = XINIM_NATIVE_SYS_EXIT,
    SYS_fork = 26,
    SYS_execve = 27,
    SYS_wait4 = 28,
    SYS_getpid = 29,
    SYS_getppid = 30,
    SYS_kill = 31,
    SYS_signal = 32,
    SYS_sigaction = 33,

    // Memory management
    SYS_brk = 34,
    SYS_mmap = 35,
    SYS_munmap = 36,
    SYS_mprotect = 37,

    // IPC (XINIM lattice IPC mapped to POSIX)
    SYS_lattice_connect = 38,
    SYS_lattice_send = 39,
    SYS_lattice_recv = 40,
    SYS_lattice_close = 41,

    // Time
    SYS_time = 42,
    SYS_gettimeofday = 43,
    SYS_clock_gettime = 44,
    SYS_nanosleep = 45,

    // User/Group IDs
    SYS_getuid = 46,
    SYS_geteuid = 47,
    SYS_getgid = 48,
    SYS_getegid = 49,
    SYS_setuid = 50,
    SYS_setgid = 51,

    // Compatibility calls needed by dietlibc-backed POSIX shells
    SYS_alarm = 52,
    SYS_getrlimit = 53,
    SYS_setrlimit = 54,
    SYS_getrusage = 55,
    SYS_setsid = 56,
    SYS_umask = 57,
    SYS_nice = 58,
    SYS_setreuid = 59,
    SYS_setregid = 60,
    SYS_rt_sigprocmask = 61,
    SYS_getpgid = 62,
    SYS_getdents = 63,
    SYS_mremap = 64,

    // Job control, file operations, and I/O multiplexing
    SYS_setpgid = 65,
    SYS_getpgrp = 66,
    SYS_getsid = 67,
    SYS_uname = 68,
    SYS_symlink = 69,
    SYS_readlink = 70,
    SYS_fchdir = 71,
    SYS_fchmod = 72,
    SYS_fchown = 73,
    SYS_truncate = 74,
    SYS_ftruncate = 75,
    SYS_readv = 76,
    SYS_writev = 77,
    SYS_select = 78,
    SYS_poll = 79,
    SYS_rt_sigreturn = 80,

    // Socket syscalls (stubs return -ENOSYS until networking is wired)
    SYS_socket = 81,
    SYS_bind = 82,
    SYS_listen = 83,
    SYS_accept = 84,
    SYS_connect = 85,
    SYS_sendto = 86,
    SYS_recvfrom = 87,
    SYS_shutdown = 88,
    SYS_setsockopt = 89,
    SYS_getsockopt = 90,
    SYS_getsockname = 91,
    SYS_getpeername = 92,
    SYS_sendmsg = 93,
    SYS_recvmsg = 94,
    SYS_socketpair = 95,

    // Additional POSIX interfaces
    SYS_sigpending = 96,
    SYS_sigsuspend = 97,
    SYS_getdents64 = 98,
    SYS_dup3 = 99,
    SYS_procinfo = 100,
    SYS_flock = 101,
    SYS_fsync = 102,
    SYS_fdatasync = 103,
    SYS_pipe2 = 104,
    SYS_sched_yield = 105,
    SYS_gettid = 106,
    SYS_statfs = 107,
    SYS_lstat = 108,
    SYS_openat = 109,
    SYS_mkdirat = 110,
    SYS_unlinkat = 111,
    SYS_set_tid_address = 112,

    // Interval timers and SysV IPC
    SYS_setitimer = 113,
    SYS_getitimer = 114,
    SYS_shmget = 115,
    SYS_shmat = 116,
    SYS_shmdt = 117,
    SYS_shmctl = 118,
    SYS_semget = 119,
    SYS_semop = 120,
    SYS_semctl = 121,
    SYS_msgget = 122,
    SYS_msgsnd = 123,
    SYS_msgrcv = 124,
    SYS_msgctl = 125,
    SYS_getgroups = 126,
    SYS_setgroups = 127,
    SYS_setresuid = 128,
    SYS_getresuid = 129,
    SYS_setresgid = 130,
    SYS_getresgid = 131,
    SYS_getpriority = 132,
    SYS_setpriority = 133,
};

typedef uint64_t (*xinim_syscall_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#ifdef __cplusplus
}
#endif
