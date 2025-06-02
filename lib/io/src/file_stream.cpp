#include "../../../include/minix/io/file_stream.hpp"
#include <algorithm> // For std::min, std::copy
#include <cstring>   // For std::memcpy, std::memchr
#include <fcntl.h>   // For O_ACCMODE, O_RDONLY, O_WRONLY, O_RDWR
#include <utility>   // For std::move

namespace minix::io {

// Static helper function (can be in an anonymous namespace or static private in class)
namespace {
    bool contains_newline(std::span<const std::byte> data) {
        return std::memchr(data.data(), '\n', data.size()) != nullptr;
    }
}

// --- Constructor and Destructor ---
FileStream::FileStream(syscall::fd_t fd, int open_flags, BufferMode mode, size_t buffer_size_override)
    : fd_(fd),
      current_state_(State::open),
      current_buffer_mode_(mode),
      open_flags_(open_flags) {
    if (fd_ == syscall::invalid_fd) {
        current_state_ = State::error;
        // No point in allocating buffer if fd is invalid
        return;
    }
    if (current_buffer_mode_ != BufferMode::none) {
        buffer_size_ = (buffer_size_override > 0) ? buffer_size_override : default_internal_buffer_size_;
        buffer_ = std::make_unique<std::byte[]>(buffer_size_);
    }
}

FileStream::~FileStream() {
    if (current_state_ != State::closed) {
        // Suppress errors in destructor, but attempt to close.
        // Errors during close in a destructor are generally not recoverable by the caller.
        (void)close();
    }
}

// --- Move Semantics ---
FileStream::FileStream(FileStream&& other) noexcept
    : fd_(std::exchange(other.fd_, syscall::invalid_fd)),
      current_state_(std::exchange(other.current_state_, State::closed)),
      current_buffer_mode_(std::exchange(other.current_buffer_mode_, BufferMode::full)),
      open_flags_(std::exchange(other.open_flags_, 0)),
      buffer_(std::move(other.buffer_)),
      buffer_size_(std::exchange(other.buffer_size_, 0)),
      buffer_pos_(std::exchange(other.buffer_pos_, 0)),
      buffer_valid_data_len_(std::exchange(other.buffer_valid_data_len_, 0)),
      buffer_is_dirty_(std::exchange(other.buffer_is_dirty_, false)) {}

FileStream& FileStream::operator=(FileStream&& other) noexcept {
    if (this != &other) {
        // Call existing close to flush any pending data, ignoring errors
        if (current_state_ != State::closed) {
            (void)close();
        }

        fd_ = std::exchange(other.fd_, syscall::invalid_fd);
        current_state_ = std::exchange(other.current_state_, State::closed);
        current_buffer_mode_ = std::exchange(other.current_buffer_mode_, BufferMode::full);
        open_flags_ = std::exchange(other.open_flags_, 0);
        buffer_ = std::move(other.buffer_);
        buffer_size_ = std::exchange(other.buffer_size_, 0);
        buffer_pos_ = std::exchange(other.buffer_pos_, 0);
        buffer_valid_data_len_ = std::exchange(other.buffer_valid_data_len_, 0);
        buffer_is_dirty_ = std::exchange(other.buffer_is_dirty_, false);
    }
    return *this;
}

// --- Static Factory Method ---
Result<StreamPtr> FileStream::open(const char* path, int posix_flags, int mode_permissions, BufferMode buff_mode, size_t buffer_size_override) {
    auto open_result = syscall::open(path, posix_flags, mode_permissions);
    if (!open_result) {
        return std::unexpected(open_result.error());
    }
    syscall::fd_t fd = open_result.value();
    if (fd == syscall::invalid_fd) {
         // This case should ideally be covered by open_result.error()
        return std::unexpected(make_error_code(IOError::bad_file_descriptor));
    }
    // Using new to allow std::make_unique with a private constructor (if it were private)
    // but since constructor is public, std::make_unique is fine.
    return std::make_unique<FileStream>(fd, posix_flags & O_ACCMODE, buff_mode, buffer_size_override);
}

// --- Private Buffer Helpers ---
Result<void> FileStream::internal_fill_buffer() noexcept {
    if (!is_readable()) {
        return std::unexpected(make_error_code(IOError::write_only));
    }
    if (buffer_is_dirty_) { // Cannot fill if write buffer is dirty
        auto flush_res = internal_flush_buffer();
        if (!flush_res) return std::unexpected(flush_res.error());
    }

    buffer_pos_ = 0;
    buffer_valid_data_len_ = 0;

    if (current_buffer_mode_ == BufferMode::none) { // Unbuffered read
        return {}; // No buffer to fill
    }

    auto read_result = syscall::read(fd_, buffer_.get(), buffer_size_);
    if (!read_result) {
        current_state_ = State::error;
        return std::unexpected(read_result.error());
    }

    buffer_valid_data_len_ = read_result.value();
    if (buffer_valid_data_len_ == 0) {
        current_state_ = State::eof; // EOF encountered
    }
    return {};
}

Result<void> FileStream::internal_flush_buffer() noexcept {
    if (!buffer_is_dirty_ || buffer_pos_ == 0) { // Nothing to flush
        buffer_is_dirty_ = false; // Ensure it's false if buffer_pos is 0
        return {};
    }
    if (!is_writable()) {
        return std::unexpected(make_error_code(IOError::read_only));
    }

    size_t to_write = buffer_pos_;
    size_t written_total = 0;

    while (written_total < to_write) {
        auto write_result = syscall::write(fd_, buffer_.get() + written_total, to_write - written_total);
        if (!write_result) {
            current_state_ = State::error;
            // Data remaining in buffer is still dirty
            // Shift unwritten data to the beginning of the buffer
            if (written_total > 0) {
                 std::copy(buffer_.get() + written_total, buffer_.get() + to_write, buffer_.get());
                 buffer_pos_ = to_write - written_total;
            }
            return std::unexpected(write_result.error());
        }
        written_total += write_result.value();
        if (write_result.value() == 0 && written_total < to_write) {
            // Wrote 0 bytes but expected to write more (e.g. disk full?)
            current_state_ = State::error;
            if (written_total > 0) {
                 std::copy(buffer_.get() + written_total, buffer_.get() + to_write, buffer_.get());
                 buffer_pos_ = to_write - written_total;
            }
            return std::unexpected(make_error_code(IOError::io_error)); // Generic I/O error
        }
    }

    buffer_pos_ = 0;
    buffer_is_dirty_ = false;
    return {};
}

void FileStream::invalidate_read_buffer() noexcept {
    buffer_pos_ = 0;
    buffer_valid_data_len_ = 0;
}

// --- Stream Interface Overrides ---
Result<size_t> FileStream::read(std::span<std::byte> data_buffer) {
    if (!is_readable() || current_state_ == State::closed) {
        return std::unexpected(make_error_code(IOError::not_open)); // Or read_only if applicable
    }
    if (current_state_ == State::error) {
        return std::unexpected(make_error_code(IOError::io_error)); // Stream in error state
    }
    if (data_buffer.empty()) return 0;

    if (current_buffer_mode_ == BufferMode::none) { // Unbuffered read
        if (buffer_is_dirty_) { // If switching from write to read, flush first
            auto flush_res = internal_flush_buffer();
            if (!flush_res) return std::unexpected(flush_res.error());
        }
        invalidate_read_buffer(); // Ensure no stale read buffer data
        auto read_result = syscall::read(fd_, data_buffer.data(), data_buffer.size());
        if (!read_result) {
            current_state_ = State::error;
            return std::unexpected(read_result.error());
        }
        if (read_result.value() == 0 && data_buffer.size() > 0) {
            current_state_ = State::eof;
        }
        return read_result;
    }

    // Buffered read
    size_t total_copied_to_user = 0;
    while (total_copied_to_user < data_buffer.size()) {
        if (buffer_pos_ >= buffer_valid_data_len_) { // Buffer empty or consumed
            if (current_state_ == State::eof) break; // Already hit EOF, nothing more to fill
            auto fill_res = internal_fill_buffer();
            if (!fill_res) { // Error during fill
                if (total_copied_to_user > 0) return total_copied_to_user; // Partial success
                return std::unexpected(fill_res.error());
            }
            if (buffer_valid_data_len_ == 0) break; // EOF reached during fill
        }

        size_t to_copy_from_buffer = std::min(data_buffer.size() - total_copied_to_user, buffer_valid_data_len_ - buffer_pos_);
        std::memcpy(data_buffer.data() + total_copied_to_user, buffer_.get() + buffer_pos_, to_copy_from_buffer);

        buffer_pos_ += to_copy_from_buffer;
        total_copied_to_user += to_copy_from_buffer;
    }
    return total_copied_to_user;
}

Result<size_t> FileStream::write(std::span<const std::byte> data) {
    if (!is_writable() || current_state_ == State::closed) {
        return std::unexpected(make_error_code(IOError::not_open)); // Or write_only if applicable
    }
     if (current_state_ == State::error) {
        return std::unexpected(make_error_code(IOError::io_error));
    }
    if (data.empty()) return 0;

    // If switching from read mode to write mode, invalidate read buffer
    if (buffer_valid_data_len_ > 0 && !buffer_is_dirty_) {
        invalidate_read_buffer();
    }

    if (current_buffer_mode_ == BufferMode::none) { // Unbuffered write
        buffer_is_dirty_ = false; // No FileStream buffer involved
        auto write_result = syscall::write(fd_, data.data(), data.size());
        if (!write_result) {
            current_state_ = State::error;
            return std::unexpected(write_result.error());
        }
        return write_result;
    }

    // Buffered write
    size_t total_data_written = 0;
    while (total_data_written < data.size()) {
        if (buffer_pos_ == buffer_size_) { // Buffer full
            auto flush_res = internal_flush_buffer();
            if (!flush_res) { // Error during flush
                if (total_data_written > 0) return total_data_written; // Partial success
                return std::unexpected(flush_res.error());
            }
        }

        size_t to_copy_to_buffer = std::min(data.size() - total_data_written, buffer_size_ - buffer_pos_);
        std::memcpy(buffer_.get() + buffer_pos_, data.data() + total_data_written, to_copy_to_buffer);

        buffer_pos_ += to_copy_to_buffer;
        total_data_written += to_copy_to_buffer;
        buffer_is_dirty_ = true;

        if (current_buffer_mode_ == BufferMode::line) {
            // Check if the newly added data contains a newline
            std::span<const std::byte> just_written_span(data.data() + total_data_written - to_copy_to_buffer, to_copy_to_buffer);
            if (contains_newline(just_written_span)) {
                auto flush_res = internal_flush_buffer();
                if (!flush_res) { // Error during line flush
                     // Even if flush fails, we've accepted the data into buffer.
                     // The error will be surfaced on next write/flush/close.
                     // For now, report what was successfully processed into our buffer.
                    if (total_data_written > 0) return total_data_written;
                    return std::unexpected(flush_res.error());
                }
            }
        }
    }
    return total_data_written;
}

Result<void> FileStream::flush() {
    if (current_state_ == State::closed) return std::unexpected(make_error_code(IOError::not_open));
    if (current_state_ == State::error) return std::unexpected(make_error_code(IOError::io_error)); // Stream already in error
    if (!buffer_is_dirty_) return {}; // Nothing to flush

    return internal_flush_buffer();
}

Result<void> FileStream::close() {
    if (current_state_ == State::closed) {
        // Optionally, allow closing an already closed stream (idempotent)
        // or return an error like "already closed". For now, idempotent.
        return {};
    }

    if (buffer_is_dirty_) {
        auto flush_res = internal_flush_buffer();
        // Even if flush fails, we must attempt to close the fd.
        // The error from flush should be reported if it occurs.
        if (!flush_res) {
            current_state_ = State::error; // Mark stream as error due to flush failure
            syscall::close(fd_); // Attempt to close fd anyway
            fd_ = syscall::invalid_fd;
            buffer_.reset(); // Release buffer
            buffer_size_ = 0;
            return std::unexpected(flush_res.error());
        }
    }

    buffer_.reset(); // Release buffer first
    buffer_size_ = 0;
    buffer_pos_ = 0;
    buffer_valid_data_len_ = 0;
    buffer_is_dirty_ = false;

    auto close_res = syscall::close(fd_);
    fd_ = syscall::invalid_fd; // Mark fd as invalid regardless of close result

    if (!close_res) {
        current_state_ = State::error;
        return std::unexpected(close_res.error());
    }

    current_state_ = State::closed;
    return {};
}

Result<size_t> FileStream::seek(std::ptrdiff_t offset, SeekDir dir) {
    if (current_state_ == State::closed || fd_ == syscall::invalid_fd) {
        return std::unexpected(make_error_code(IOError::not_open));
    }
     if (current_state_ == State::error && dir != SeekDir::beg && offset != 0) { // Allow seeking to 0 on error stream? Maybe not.
        return std::unexpected(make_error_code(IOError::io_error));
    }

    // If buffer is dirty, flush it first
    if (buffer_is_dirty_) {
        auto flush_res = internal_flush_buffer();
        if (!flush_res) {
            return std::unexpected(flush_res.error());
        }
    }
    // Invalidate read buffer content after seek
    invalidate_read_buffer();

    syscall::LseekWhence whence;
    switch(dir) {
        case SeekDir::beg: whence = syscall::LseekWhence::set; break;
        case SeekDir::cur: whence = syscall::LseekWhence::cur; break;
        case SeekDir::end: whence = syscall::LseekWhence::end; break;
        default: return std::unexpected(make_error_code(IOError::invalid_argument));
    }

    auto seek_res = syscall::lseek(fd_, offset, whence);
    if (!seek_res) {
        current_state_ = State::error; // Or some other state?
        return std::unexpected(seek_res.error());
    }

    // After a successful seek, we are no longer at EOF (if we were)
    if (current_state_ == State::eof) {
       current_state_ = State::open;
    }
    return seek_res.value();
}

Result<size_t> FileStream::tell() const {
    if (current_state_ == State::closed || fd_ == syscall::invalid_fd) {
        return std::unexpected(make_error_code(IOError::not_open));
    }
    if (current_state_ == State::error) { // Telling position on an error stream might be unreliable
         return std::unexpected(make_error_code(IOError::io_error));
    }

    auto current_pos_syscall = syscall::lseek(fd_, 0, syscall::LseekWhence::cur);
    if (!current_pos_syscall) {
        // ((FileStream*)this)->current_state_ = State::error; // const method
        return std::unexpected(current_pos_syscall.error());
    }

    size_t final_pos = current_pos_syscall.value();

    if (buffer_is_dirty_) { // Write buffer has pending data
        final_pos += buffer_pos_; // buffer_pos_ is end of data to write
    } else { // Read buffer might have unconsumed data or was just filled
        if (buffer_valid_data_len_ > 0) { // Data in read buffer
            final_pos -= (buffer_valid_data_len_ - buffer_pos_);
        }
    }
    return final_pos;
}

Stream::State FileStream::state() const noexcept {
    return current_state_;
}

bool FileStream::is_readable() const noexcept {
    if (current_state_ == State::closed || current_state_ == State::error) return false;
    return (open_flags_ == O_RDONLY || open_flags_ == O_RDWR);
}

bool FileStream::is_writable() const noexcept {
    if (current_state_ == State::closed || current_state_ == State::error) return false;
    return (open_flags_ == O_WRONLY || open_flags_ == O_RDWR);
}

Result<void> FileStream::set_buffer_mode(BufferMode mode) {
    if (current_state_ == State::closed) return std::unexpected(make_error_code(IOError::not_open));

    if (current_buffer_mode_ == mode) return {}; // No change

    // Flush existing buffer before changing mode
    if (buffer_is_dirty_) {
        auto flush_res = internal_flush_buffer();
        if (!flush_res) return std::unexpected(flush_res.error());
    }
    invalidate_read_buffer(); // Also clear read buffer state

    current_buffer_mode_ = mode;
    if (mode == BufferMode::none) {
        buffer_.reset(); // Deallocate buffer
        buffer_size_ = 0;
    } else {
        if (!buffer_) { // If buffer was previously none, allocate it
            buffer_size_ = default_internal_buffer_size_; // Or last known size if we stored it
            buffer_ = std::make_unique<std::byte[]>(buffer_size_);
        }
    }
    buffer_pos_ = 0;
    buffer_valid_data_len_ = 0;
    buffer_is_dirty_ = false;
    return {};
}

Stream::BufferMode FileStream::buffer_mode() const noexcept {
    return current_buffer_mode_;
}

} // namespace minix::io
