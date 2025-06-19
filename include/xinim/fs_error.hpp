#pragma once

#include "../../h/error.hpp" // For ErrorCode enum
#include <system_error>    // For std::error_category, std::error_code
#include <string>          // For std::string

// Forward declaration if ErrorCode is in a namespace, e.g. minix::fs
// Assuming ErrorCode is in the global namespace based on h/error.hpp structure.
// If ErrorCode is in a namespace like `minix`, then the make_error_code overload
// and is_error_code_enum specialization should also be in that namespace or correctly qualified.

class MinixFsErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override {
        return "minix_fs";
    }

    std::string message(int condition) const override {
        switch (static_cast<ErrorCode>(condition)) {
            // POSIX-like FS errors
            case ErrorCode::EPERM:
                return "Operation not permitted";
            case ErrorCode::ENOENT:
                return "No such file or directory";
            case ErrorCode::ESRCH:
                return "No such process";
            case ErrorCode::EINTR:
                return "Interrupted system call";
            case ErrorCode::EIO:
                return "I/O error";
            case ErrorCode::ENXIO:
                return "No such device or address";
            case ErrorCode::E2BIG:
                return "Argument list too long";
            case ErrorCode::ENOEXEC:
                return "Exec format error";
            case ErrorCode::EBADF:
                return "Bad file number";
            case ErrorCode::ECHILD:
                return "No child processes";
            case ErrorCode::EAGAIN: // Also EWOULDBLOCK
                return "Try again (Resource temporarily unavailable)";
            case ErrorCode::ENOMEM:
                return "Out of memory";
            case ErrorCode::EACCES:
                return "Permission denied";
            case ErrorCode::EFAULT:
                return "Bad address";
            case ErrorCode::ENOTBLK:
                return "Block device required";
            case ErrorCode::EBUSY:
                return "Device or resource busy";
            case ErrorCode::EEXIST:
                return "File exists";
            case ErrorCode::EXDEV:
                return "Cross-device link";
            case ErrorCode::ENODEV:
                return "No such device";
            case ErrorCode::ENOTDIR:
                return "Not a directory";
            case ErrorCode::EISDIR:
                return "Is a directory";
            case ErrorCode::EINVAL:
                return "Invalid argument";
            case ErrorCode::ENFILE:
                return "File table overflow";
            case ErrorCode::EMFILE:
                return "Too many open files";
            case ErrorCode::ENOTTY:
                return "Not a typewriter (Inappropriate I/O control operation)";
            case ErrorCode::ETXTBSY:
                return "Text file busy";
            case ErrorCode::EFBIG:
                return "File too large";
            case ErrorCode::ENOSPC:
                return "No space left on device";
            case ErrorCode::ESPIPE:
                return "Illegal seek";
            case ErrorCode::EROFS:
                return "Read-only file system";
            case ErrorCode::EMLINK:
                return "Too many links";
            case ErrorCode::EPIPE:
                return "Broken pipe";
            case ErrorCode::EDOM:
                return "Math argument out of domain of func";
            case ErrorCode::ERANGE:
                return "Math result not representable";

            // Minix FS specific errors
            case ErrorCode::E_LOCKED:
                return "Table locked";
            case ErrorCode::E_BAD_CALL: // Note: E_BAD_CALL is not in h/error.hpp's ErrorCode enum
                return "Bad system call (FS context)"; // Placeholder if it were
            case ErrorCode::E_LONG_STRING:
                return "String is too long";
            case ErrorCode::EOF_ERROR: // Note: EOF_ERROR is -104, not in main block
                return "End of file detected by driver";

            // Kernel-generated errors (these might need their own category if treated distinctly from FS errors with same value)
            // For now, providing generic messages if their values overlap with FS errors.
            // Example: EPERM and E_BAD_DEST are both -1. Context is key.
            // This category is "minix_fs", so these might be less relevant here or need namespacing.
            // For the purpose of this exercise, I'll only list a few that don't directly overlap with common FS ones shown above.
            // case ErrorCode::E_TRY_AGAIN: (overlaps EAGAIN)
            //     return "Kernel tables full, try again";
            // case ErrorCode::E_NO_MESSAGE:
            //     return "Kernel: No message present for receive";


            default:
                return "Unknown Minix FS error";
        }
    }
};

// Singleton accessor for the Minix FS error category
inline const std::error_category& minix_fs_category() {
    static MinixFsErrorCategory instance;
    return instance;
}

// Overload std::make_error_code for ErrorCode
// This should ideally be in the same namespace as ErrorCode if it's namespaced.
// Assuming ErrorCode is global for now as per h/error.hpp structure.
inline std::error_code make_error_code(ErrorCode e) {
    return {static_cast<int>(e), minix_fs_category()};
}

namespace std {
    // Specialization of is_error_code_enum for ErrorCode
    template <>
    struct is_error_code_enum<ErrorCode> : true_type {};
}
