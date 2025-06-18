#include "minix/io/file_operations.hpp"
#include "minix/io/file_stream.hpp"
#include "minix/io/standard_streams.hpp"

#include <array>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <unistd.h>

namespace minix::io {

/// Open a file path and return a stream for I/O operations.
///
/// \param path  Filesystem path to open.
/// \param mode  Combination of OpenMode flags controlling access.
/// \param perms File permission bits applied when creating the file.
/// \return      A newly allocated FileStream on success or an error code.
Result<StreamPtr> open_stream(std::string_view path, OpenMode mode, Permissions perms) {
    std::array<char, 256> buf{};
    if (path.size() >= buf.size()) {
        return std::unexpected(std::make_error_code(std::errc::file_too_large));
    }
    std::memcpy(buf.data(), path.data(), path.size());
    int flags = 0;
    if ((mode & OpenMode::read) && (mode & OpenMode::write)) {
        flags |= O_RDWR;
    } else if (mode & OpenMode::read) {
        flags |= O_RDONLY;
    } else if (mode & OpenMode::write) {
        flags |= O_WRONLY;
    }
    if (mode & OpenMode::create)
        flags |= O_CREAT;
    if (mode & OpenMode::exclusive)
        flags |= O_EXCL;
    if (mode & OpenMode::truncate)
        flags |= O_TRUNC;
    if (mode & OpenMode::append)
        flags |= O_APPEND;

    int fd = ::open(buf.data(), flags, perms.mode);
    if (fd < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }
    bool write_ok = static_cast<unsigned>(mode) & static_cast<unsigned>(OpenMode::write);
    StreamPtr ptr = std::make_unique<FileStream>(fd, write_ok);
    return Result<StreamPtr>(std::move(ptr));
}

/// Convenience wrapper around open_stream that always creates/truncates the
/// file for writing.
///
/// \param path  Filesystem path to create.
/// \param perms File permission bits for the new file.
/// \return      A FileStream opened for writing or an error code.
Result<StreamPtr> create_stream(std::string_view path, Permissions perms) {
    return open_stream(path, OpenMode::write | OpenMode::create | OpenMode::truncate, perms);
}

} // namespace minix::io
