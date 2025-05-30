#pragma once
// Modernized for C++17

// Number of system calls allowed.
constexpr int NCALLS = 69;

// Enumeration of all kernel-visible system calls.
enum class SysCall : int {
    EXIT = 1,    ///< exit
    FORK = 2,    ///< fork
    READ = 3,    ///< read
    WRITE = 4,   ///< write
    OPEN = 5,    ///< open
    CLOSE = 6,   ///< close
    WAIT = 7,    ///< wait
    CREAT = 8,   ///< creat
    LINK = 9,    ///< link
    UNLINK = 10, ///< unlink
    CHDIR = 12,  ///< change directory
    TIME = 13,   ///< get time
    MKNOD = 14,  ///< make node
    CHMOD = 15,  ///< change mode
    CHOWN = 16,  ///< change owner
    BRK = 17,    ///< adjust data segment
    STAT = 18,   ///< get file status
    LSEEK = 19,  ///< seek
    GETPID = 20, ///< get process id
    MOUNT = 21,  ///< mount file system
    UMOUNT = 22, ///< unmount file system
    SETUID = 23, ///< set user id
    GETUID = 24, ///< get user id
    STIME = 25,  ///< set time
    ALARM = 27,  ///< alarm clock
    FSTAT = 28,  ///< file status
    PAUSE = 29,  ///< pause
    UTIME = 30,  ///< change file times
    ACCESS = 33, ///< access check
    SYNC = 36,   ///< sync disks
    KILL = 37,   ///< kill process
    DUP = 41,    ///< duplicate file descriptor
    PIPE = 42,   ///< create pipe
    TIMES = 43,  ///< process times
    SETGID = 46, ///< set group id
    GETGID = 47, ///< get group id
    SIGNAL = 48, ///< signal operations
    IOCTL = 54,  ///< device ioctl
    EXEC = 59,   ///< execute program
    UMASK = 60,  ///< set file creation mask
    CHROOT = 61, ///< change root

    // Processed like system calls but not part of the user API.
    KSIG = 64,      ///< kernel detected signal
    UNPAUSE = 65,   ///< check for EINTR
    BRK2 = 66,      ///< memory map setup
    REVIVE = 67,    ///< revive sleeping process
    TASK_REPLY = 68 ///< tty task reply
};
