#pragma once

/**
 * @file file_operations.hpp
 * @brief Helpers for opening files as Stream instances.
 */

#include "stream.hpp"
#include <string_view>

namespace minix::io {

/// Flags controlling how a file is opened.
enum class OpenMode : unsigned {
    read = 1 << 0,
    write = 1 << 1,
    create = 1 << 2,
    exclusive = 1 << 3,
    truncate = 1 << 4,
    append = 1 << 5
};

inline OpenMode operator|(OpenMode a, OpenMode b) {
    return static_cast<OpenMode>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

inline bool operator&(OpenMode a, OpenMode b) {
    return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0;
}

/// Permission bits used when creating new files.
struct Permissions {
    unsigned mode{0644}; ///< POSIX mode bits
};

/**
 * @brief Open a file path and return a Stream.
 *
 * @param path  Filesystem path to open.
 * @param mode  Combination of OpenMode flags.
 * @param perms Permissions applied when creating the file.
 * @return Unique pointer to a Stream or an error code.
 */
Result<StreamPtr> open_stream(std::string_view path, OpenMode mode, Permissions perms = {});

/**
 * @brief Create or truncate a file for writing.
 *
 * @param path  Filesystem path to create.
 * @param perms File permissions for the new file.
 * @return Stream for the opened file or an error code.
 */
Result<StreamPtr> create_stream(std::string_view path, Permissions perms = {});

} // namespace minix::io
