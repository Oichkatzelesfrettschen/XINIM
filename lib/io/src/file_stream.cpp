#include "minix/io/file_stream.hpp"

#include <cerrno>
#include <cstring>
#include <expected> // For std::unexpected
#include <fcntl.h>
#include <unistd.h>

namespace minix::io {

/// Read from the underlying file descriptor into the supplied buffer.
///
/// \param buffer Destination for file data.
/// \param length Maximum number of bytes to read.
/// \return       Number of bytes read or an error code.
Result<size_t> FileStream::read(std::byte *buffer, size_t length) {
    ssize_t ret = ::read(fd_, buffer, length);
    if (ret < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }
    return static_cast<size_t>(ret);
}

/// Write the given data to the underlying file descriptor.
///
/// \param buffer Data to write.
/// \param length Number of bytes to write.
/// \return       Number of bytes written or an error code.
Result<size_t> FileStream::write(const std::byte *buffer, size_t length) {
    ssize_t ret = ::write(fd_, buffer, length);
    if (ret < 0) {
        return std::unexpected(std::error_code(errno, std::generic_category()));
    }
    return static_cast<size_t>(ret);
}

/// Close the underlying file descriptor if open.
///
/// \return Error code describing any failure.
std::error_code FileStream::close() {
    if (fd_ >= 0) {
        if (::close(fd_) < 0) {
            return std::error_code(errno, std::generic_category());
        }
        fd_ = -1;
    }
    return {};
}

} // namespace minix::io
