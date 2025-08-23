#include "../include/lib.hpp" // C++23 header

#include <filesystem>
#include <system_error>
#include <unistd.h>

/**
 * @file access.cpp
 * @brief C++23 wrapper providing permission checks for ::access.
 *
 * The function performs a preliminary permission check using
 * `std::filesystem` before delegating to the MINIX file system
 * server via @c callm3.  This mirrors the POSIX ``access``
 * semantics while embracing modern C++ facilities.
 */

/**
 * @brief Test a file's accessibility.
 *
 * @param name Path to the file or directory being examined.
 * @param mode Permission mask composed of ::F_OK, ::R_OK, ::W_OK, and ::X_OK.
 * @return ``0`` on success or ``-1`` on failure with ``errno`` set.
 */
int access(const char *name, int mode) {
    namespace fs = std::filesystem;

    // Query the file status and short-circuit on errors.
    std::error_code ec;
    fs::file_status st = fs::status(name, ec);
    if (ec) {
        errno = ec.value();
        return -1;
    }

    // For pure existence checks (F_OK), status is sufficient.
    if (mode != F_OK) {
        const fs::perms p = st.permissions();

        if ((mode & R_OK) && (p & fs::perms::owner_read) == fs::perms::none) {
            errno = -static_cast<int>(ErrorCode::EACCES);
            return -1;
        }

        if ((mode & W_OK) && (p & fs::perms::owner_write) == fs::perms::none) {
            errno = -static_cast<int>(ErrorCode::EACCES);
            return -1;
        }

        if ((mode & X_OK) && (p & fs::perms::owner_exec) == fs::perms::none) {
            errno = -static_cast<int>(ErrorCode::EACCES);
            return -1;
        }
    }

    // Delegate to the file system server for additional checks and side effects.
    return callm3(FS, ACCESS, mode, const_cast<char *>(name));
}
