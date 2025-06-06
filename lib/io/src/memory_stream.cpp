#include "minix/io/memory_stream.hpp"

#include <cstring>

namespace minix::io {

// Read up to ``length`` bytes from ``buffer_`` into ``buffer``.
Result<size_t> MemoryStream::read(std::byte *buffer, size_t length) {
    size_t available = buffer_.size() > pos_ ? buffer_.size() - pos_ : 0;
    size_t to_read = std::min(length, available);
    if (to_read > 0) {
        std::memcpy(buffer, buffer_.data() + pos_, to_read);
        pos_ += to_read;
    }
    return {to_read, {}};
}

// Write ``length`` bytes from ``buffer`` into ``buffer_`` at ``pos_``.
Result<size_t> MemoryStream::write(const std::byte *buffer, size_t length) {
    size_t end = pos_ + length;
    if (end > buffer_.size()) {
        buffer_.resize(end);
    }
    std::memcpy(buffer_.data() + pos_, buffer, length);
    pos_ += length;
    return {length, {}};
}

void MemoryStream::seek(size_t pos) { pos_ = std::min(pos, buffer_.size()); }

} // namespace minix::io
