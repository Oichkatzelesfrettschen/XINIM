#pragma once
#include <stdint.h>

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
  SYS_exit = 25,
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
};

typedef uint64_t (*xinim_syscall_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#ifdef __cplusplus
}
#endif

