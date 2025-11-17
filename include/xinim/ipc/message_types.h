/**
 * @file message_types.h
 * @brief Message type constants for XINIM Lattice IPC
 *
 * This header defines all message types used for communication between
 * the kernel and userspace servers (VFS, Process Manager, Memory Manager).
 * Message types are organized in ranges to prevent collisions.
 *
 * @ingroup ipc
 */

#ifndef XINIM_IPC_MESSAGE_TYPES_H
#define XINIM_IPC_MESSAGE_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Message Type Ranges:
 *
 * 0-99:     Kernel-only messages (reserved)
 * 100-199:  VFS Server messages
 * 200-299:  Process Manager messages
 * 300-399:  Memory Manager messages
 * 400-499:  Network Stack messages (future)
 * 500-599:  Security/Capability Server messages (future)
 * 600-699:  Time/Clock Server messages (future)
 * 1000+:    User-defined IPC messages
 */

/* ========================================================================
 * VFS Server Message Types (100-199)
 * ======================================================================== */

/* File I/O Operations (100-119) */
#define VFS_OPEN            100  /**< Open file */
#define VFS_CLOSE           101  /**< Close file */
#define VFS_READ            102  /**< Read from file */
#define VFS_WRITE           103  /**< Write to file */
#define VFS_LSEEK           104  /**< Seek within file */
#define VFS_STAT            105  /**< Get file status */
#define VFS_FSTAT           106  /**< Get file status by fd */
#define VFS_ACCESS          107  /**< Check file accessibility */
#define VFS_DUP             108  /**< Duplicate file descriptor */
#define VFS_DUP2            109  /**< Duplicate file descriptor to specific fd */
#define VFS_PIPE            110  /**< Create pipe */
#define VFS_IOCTL           111  /**< Device I/O control */
#define VFS_FCNTL           112  /**< File control */

/* Directory Operations (120-139) */
#define VFS_MKDIR           120  /**< Create directory */
#define VFS_RMDIR           121  /**< Remove directory */
#define VFS_CHDIR           122  /**< Change current directory */
#define VFS_GETCWD          123  /**< Get current working directory */
#define VFS_LINK            124  /**< Create hard link */
#define VFS_UNLINK          125  /**< Remove file/link */
#define VFS_RENAME          126  /**< Rename file/directory */
#define VFS_CHMOD           127  /**< Change file mode */
#define VFS_CHOWN           128  /**< Change file owner */
#define VFS_OPENDIR         129  /**< Open directory for reading */
#define VFS_READDIR         130  /**< Read directory entry */
#define VFS_CLOSEDIR        131  /**< Close directory */

/* Advanced File Operations (140-159) */
#define VFS_SYMLINK         140  /**< Create symbolic link */
#define VFS_READLINK        141  /**< Read symbolic link */
#define VFS_TRUNCATE        142  /**< Truncate file */
#define VFS_FTRUNCATE       143  /**< Truncate file by fd */
#define VFS_SYNC            144  /**< Sync filesystem */
#define VFS_FSYNC           145  /**< Sync file to disk */
#define VFS_FDATASYNC       146  /**< Sync file data to disk */

/* Response messages (190-199) */
#define VFS_REPLY           190  /**< Generic VFS reply */
#define VFS_ERROR           191  /**< VFS error response */

/* ========================================================================
 * Process Manager Message Types (200-299)
 * ======================================================================== */

/* Process Lifecycle (200-219) */
#define PROC_FORK           200  /**< Fork process */
#define PROC_EXEC           201  /**< Execute program */
#define PROC_EXIT           202  /**< Process exit */
#define PROC_WAIT           203  /**< Wait for child process */
#define PROC_KILL           204  /**< Send signal to process */
#define PROC_GETPID         205  /**< Get process ID */
#define PROC_GETPPID        206  /**< Get parent process ID */
#define PROC_SETPGID        207  /**< Set process group ID */
#define PROC_GETPGID        208  /**< Get process group ID */
#define PROC_SETSID         209  /**< Create new session */

/* Signal Handling (220-239) */
#define PROC_SIGNAL         220  /**< Set signal handler */
#define PROC_SIGACTION      221  /**< Advanced signal handling */
#define PROC_SIGPROCMASK    222  /**< Set signal mask */
#define PROC_SIGPENDING     223  /**< Get pending signals */
#define PROC_SIGSUSPEND     224  /**< Suspend until signal */
#define PROC_SIGRETURN      225  /**< Return from signal handler */

/* Process Attributes (240-259) */
#define PROC_GETUID         240  /**< Get user ID */
#define PROC_GETEUID        241  /**< Get effective user ID */
#define PROC_GETGID         242  /**< Get group ID */
#define PROC_GETEGID        243  /**< Get effective group ID */
#define PROC_SETUID         244  /**< Set user ID */
#define PROC_SETGID         245  /**< Set group ID */
#define PROC_SETREUID       246  /**< Set real and effective user ID */
#define PROC_SETREGID       247  /**< Set real and effective group ID */
#define PROC_GETGROUPS      248  /**< Get supplementary groups */
#define PROC_SETGROUPS      249  /**< Set supplementary groups */

/* Process Information (260-279) */
#define PROC_GETRUSAGE      260  /**< Get resource usage */
#define PROC_GETRLIMIT      261  /**< Get resource limits */
#define PROC_SETRLIMIT      262  /**< Set resource limits */
#define PROC_GETPRIORITY    263  /**< Get scheduling priority */
#define PROC_SETPRIORITY    264  /**< Set scheduling priority */

/* Response messages (290-299) */
#define PROC_REPLY          290  /**< Generic process manager reply */
#define PROC_ERROR          291  /**< Process manager error response */

/* ========================================================================
 * Memory Manager Message Types (300-399)
 * ======================================================================== */

/* Heap Management (300-319) */
#define MM_BRK              300  /**< Set program break (heap) */
#define MM_SBRK             301  /**< Increment program break */

/* Memory Mapping (320-349) */
#define MM_MMAP             320  /**< Map memory region */
#define MM_MUNMAP           321  /**< Unmap memory region */
#define MM_MPROTECT         322  /**< Change memory protection */
#define MM_MSYNC            323  /**< Sync memory mapping to file */
#define MM_MLOCK            324  /**< Lock memory pages */
#define MM_MUNLOCK          325  /**< Unlock memory pages */
#define MM_MLOCKALL         326  /**< Lock all memory pages */
#define MM_MUNLOCKALL       327  /**< Unlock all memory pages */
#define MM_MADVISE          328  /**< Give advice about memory usage */
#define MM_MINCORE          329  /**< Check if pages are in memory */

/* Shared Memory (350-369) */
#define MM_SHMGET           350  /**< Get shared memory segment */
#define MM_SHMAT            351  /**< Attach shared memory */
#define MM_SHMDT            352  /**< Detach shared memory */
#define MM_SHMCTL           353  /**< Shared memory control */

/* Memory Information (370-389) */
#define MM_GETPAGESIZE      370  /**< Get page size */
#define MM_GETPHYSPAGES     371  /**< Get physical page count */
#define MM_GETAVAILPAGES    372  /**< Get available page count */

/* Response messages (390-399) */
#define MM_REPLY            390  /**< Generic memory manager reply */
#define MM_ERROR            391  /**< Memory manager error response */

/* ========================================================================
 * Well-Known Server PIDs
 * ======================================================================== */

#define VFS_SERVER_PID      2    /**< VFS server PID */
#define PROC_MGR_PID        3    /**< Process manager PID */
#define MEM_MGR_PID         4    /**< Memory manager PID */
#define INIT_PID            1    /**< Init process PID */

/* ========================================================================
 * IPC Error Codes (aligned with POSIX errno)
 * ======================================================================== */

#define IPC_SUCCESS         0    /**< Success */
#define IPC_EPERM           1    /**< Operation not permitted */
#define IPC_ENOENT          2    /**< No such file or directory */
#define IPC_ESRCH           3    /**< No such process */
#define IPC_EINTR           4    /**< Interrupted system call */
#define IPC_EIO             5    /**< I/O error */
#define IPC_ENXIO           6    /**< No such device or address */
#define IPC_E2BIG           7    /**< Argument list too long */
#define IPC_ENOEXEC         8    /**< Exec format error */
#define IPC_EBADF           9    /**< Bad file descriptor */
#define IPC_ECHILD          10   /**< No child processes */
#define IPC_EAGAIN          11   /**< Try again */
#define IPC_ENOMEM          12   /**< Out of memory */
#define IPC_EACCES          13   /**< Permission denied */
#define IPC_EFAULT          14   /**< Bad address */
#define IPC_EBUSY           16   /**< Device or resource busy */
#define IPC_EEXIST          17   /**< File exists */
#define IPC_EXDEV           18   /**< Cross-device link */
#define IPC_ENODEV          19   /**< No such device */
#define IPC_ENOTDIR         20   /**< Not a directory */
#define IPC_EISDIR          21   /**< Is a directory */
#define IPC_EINVAL          22   /**< Invalid argument */
#define IPC_ENFILE          23   /**< File table overflow */
#define IPC_EMFILE          24   /**< Too many open files */
#define IPC_ENOTTY          25   /**< Not a typewriter */
#define IPC_ETXTBSY         26   /**< Text file busy */
#define IPC_EFBIG           27   /**< File too large */
#define IPC_ENOSPC          28   /**< No space left on device */
#define IPC_ESPIPE          29   /**< Illegal seek */
#define IPC_EROFS           30   /**< Read-only file system */
#define IPC_EMLINK          31   /**< Too many links */
#define IPC_EPIPE           32   /**< Broken pipe */
#define IPC_EDOM            33   /**< Math argument out of domain */
#define IPC_ERANGE          34   /**< Math result not representable */
#define IPC_EDEADLK         35   /**< Resource deadlock would occur */
#define IPC_ENAMETOOLONG    36   /**< File name too long */
#define IPC_ENOLCK          37   /**< No record locks available */
#define IPC_ENOSYS          38   /**< Function not implemented */
#define IPC_ENOTEMPTY       39   /**< Directory not empty */

#ifdef __cplusplus
}
#endif

#endif /* XINIM_IPC_MESSAGE_TYPES_H */
