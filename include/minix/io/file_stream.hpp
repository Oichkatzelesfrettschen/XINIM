#pragma once

/**
 * @file file_stream.hpp
 * @brief Stream implementation backed by a file descriptor.
 */

#include "stream.hpp"

namespace minix::io {

/**
 * @brief Stream backed by a UNIX file descriptor.
 */
class FileStream : public Stream {
  public:
    /// Construct from an existing descriptor.
    /// @param fd     File descriptor to manage.
    /// @param write  Whether writing is permitted.
    explicit FileStream(int fd, bool write) : fd_(fd), writable_(write) {}
    ~FileStream() override = default;

    /// Read data from the file descriptor.
    Result<size_t> read(std::span<std::byte> buffer) override;
    /// Write data to the file descriptor.
    Result<size_t> write(std::span<const std::byte> buffer) override;
    /// Close the file descriptor if owned.
    std::error_code close() override;
    /// Access the underlying descriptor.
    int descriptor() const override { return fd_; }

  private:
    int fd_{-1};           ///< Managed descriptor
    bool writable_{false}; ///< Whether writes are allowed
};

} // namespace minix::io
