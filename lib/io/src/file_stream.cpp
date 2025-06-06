#include "minix/io/file_stream.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <span>
#include <unistd.h>

namespace minix::io {

Result<size_t> FileStream::read(std::span<std::byte> buffer) {
    ssize_t ret = ::read(fd_, buffer.data(), buffer.size());
    if (ret < 0) {
        return {std::nullopt, std::error_code(errno, std::generic_category())};
    }
    return {static_cast<size_t>(ret), {}};
}

Result<size_t> FileStream::write(std::span<const std::byte> buffer) {
    ssize_t ret = ::write(fd_, buffer.data(), buffer.size());
    if (ret < 0) {
        return {std::nullopt, std::error_code(errno, std::generic_category())};
    }
    return {static_cast<size_t>(ret), {}};
}

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
