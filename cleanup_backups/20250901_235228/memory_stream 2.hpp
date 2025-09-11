#pragma once

/**
 * @file memory_stream.hpp
 * @brief Simple in-memory Stream implementation.
 */

#include "stream.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace minix::io {

/// In-memory implementation of Stream for testing and buffering.
class MemoryStream : public Stream {
  public:
    MemoryStream() = default;
    ~MemoryStream() override = default;

    /// Read from the internal buffer starting at the current position.
    Result<size_t> read(std::byte *buffer, size_t length) override;

    /// Write to the internal buffer starting at the current position.
    Result<size_t> write(const std::byte *buffer, size_t length) override;

    /// Reset the current position to a new value.
    void seek(size_t pos);

    /// Access the full buffer content.
    const std::vector<std::byte> &data() const { return buffer_; }

    int descriptor() const override { return -1; }

  private:
    std::vector<std::byte> buffer_{}; ///< backing storage
    size_t pos_{0};                   ///< current cursor
};

} // namespace minix::io
