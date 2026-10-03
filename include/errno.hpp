#pragma once
// Errno definitions converted to modern C++ enum class style.
// Each enumerator documents the standard meaning.

constexpr int OK = 0;    // successful return status
constexpr int ERROR = 1; // generic failure code

// POSIX-style error numbers used throughout user space.
// Values match historic MINIX definitions.
enum class Errno : int {
    EPERM = 1,    // Operation not permitted
    ENOENT = 2,   // No such file or directory
    ESRCH = 3,    // No such process
    EINTR = 4,    // Interrupted system call
    EIO = 5,      // I/O error
    ENXIO = 6,    // No such device or address
    E2BIG = 7,    // Argument list too long
    ENOEXEC = 8,  // Exec format error
    EBADF = 9,    // Bad file number
    ECHILD = 10,  // No child processes
    EAGAIN = 11,  // Try again
    ENOMEM = 12,  // Out of memory
    EACCES = 13,  // Permission denied
    EFAULT = 14,  // Bad address
    ENOTBLK = 15, // Block device required
    EBUSY = 16,   // Device or resource busy
    EEXIST = 17,  // File exists
    EXDEV = 18,   // Cross-device link
    ENODEV = 19,  // No such device
    ENOTDIR = 20, // Not a directory
    EISDIR = 21,  // Is a directory
    EINVAL = 22,  // Invalid argument
    ENFILE = 23,  // File table overflow
    EMFILE = 24,  // Too many open files
    ENOTTY = 25,  // Not a typewriter
    ETXTBSY = 26, // Text file busy
    EFBIG = 27,   // File too large
    ENOSPC = 28,  // No space left on device
    ESPIPE = 29,  // Illegal seek
    EROFS = 30,   // Read-only file system
    EMLINK = 31,  // Too many links
    EPIPE = 32,   // Broken pipe
    EDOM = 33,    // Math argument out of domain
    ERANGE = 34,  // Result too large

    E_LOCKED = 101,     // Table locked
    E_BAD_CALL = 102,   // Bad system call
    E_LONG_STRING = 103 // String is too long
};
