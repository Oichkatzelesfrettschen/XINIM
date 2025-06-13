#include "minix/io/memory_stream.hpp"

#include <cstring>

namespace minix::io {

/// Read from the internal buffer starting at the current seek position.
///
/// \param buffer Destination buffer.
/// \param length Maximum number of bytes to read.
/// \return       Number of bytes actually read.
Result<size_t> MemoryStream::read(std::byte *buffer, size_t length) {
    size_t available = buffer_.size() > pos_ ? buffer_.size() - pos_ : 0;
    size_t to_read = std::min(length, available);
    if (to_read > 0) {
        std::memcpy(buffer, buffer_.data() + pos_, to_read);
        pos_ += to_read;
    }
    return to_read;
}

/// Write to the internal buffer starting at the current seek position.
///
/// \param buffer Source data to write.
/// \param length Number of bytes to write.
/// \return       Number of bytes written.
Result<size_t> MemoryStream::write(const std::byte *buffer, size_t length) {
    size_t end = pos_ + length;
    if (end > buffer_.size()) {
        buffer_.resize(end);
    }
    std::memcpy(buffer_.data() + pos_, buffer, length);
    pos_ += length;
    return length;
}

/// Set the current seek position within the buffer.
///
/// \param pos New offset relative to the start of the buffer.
void MemoryStream::seek(size_t pos) { pos_ = std::min(pos, buffer_.size()); }

} // namespace minix::io
