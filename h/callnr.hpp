#pragma once
// Modernized for C++23

#include "../include/defs.hpp"    // Added for consistency and potential common types
inline constexpr int NCALLS = 69; /* number of system calls allowed */
inline constexpr int EXIT = 1;
inline constexpr int FORK = 2;
inline constexpr int READ = 3;
inline constexpr int WRITE = 4;
inline constexpr int OPEN = 5;
inline constexpr int CLOSE = 6;
inline constexpr int WAIT = 7;
inline constexpr int CREAT = 8;
inline constexpr int LINK = 9;
inline constexpr int UNLINK = 10;
inline constexpr int CHDIR = 12;
inline constexpr int TIME = 13;
inline constexpr int MKNOD = 14;
inline constexpr int CHMOD = 15;
inline constexpr int CHOWN = 16;
inline constexpr int BRK = 17;
inline constexpr int STAT = 18;
inline constexpr int LSEEK = 19;
inline constexpr int GETPID = 20;
inline constexpr int MOUNT = 21;
inline constexpr int UMOUNT = 22;
inline constexpr int SETUID = 23;
inline constexpr int GETUID = 24;
inline constexpr int STIME = 25;
inline constexpr int ALARM = 27;
inline constexpr int FSTAT = 28;
inline constexpr int PAUSE = 29;
inline constexpr int UTIME = 30;
inline constexpr int ACCESS = 33;
inline constexpr int SYNC = 36;
inline constexpr int KILL = 37;
inline constexpr int DUP = 41;
inline constexpr int PIPE = 42;
inline constexpr int TIMES = 43;
inline constexpr int SETGID = 46;
inline constexpr int GETGID = 47;
inline constexpr int SIGNAL = 48;
inline constexpr int IOCTL = 54;
inline constexpr int EXEC = 59;
inline constexpr int UMASK = 60;
inline constexpr int CHROOT = 61;

/* The following are not system calls, but are processed like them. */
inline constexpr int KSIG = 64;       /* kernel detected a signal */
inline constexpr int UNPAUSE = 65;    /* to MM or FS: check for ErrorCode::EINTR */
inline constexpr int BRK2 = 66;       /* to MM: used to say how big FS & INIT are */
inline constexpr int REVIVE = 67;     /* to FS: revive a sleeping process */
inline constexpr int TASK_REPLY = 68; /* to FS: reply code from tty task */
