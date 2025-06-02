#include "../../include/io/stream.hpp"
#include <algorithm> // For std::copy, std::fill, std::min
#include <cstring>   // For std::memcpy, memchr
#include <utility>   // For std::exchange

// This file should contain implementations for syscalls, or include a header that does.
// For now, defining stubs here.
// #include "syscall_wrappers.hpp"

namespace minix::io {

    // Syscall stubs (to be replaced by actual syscall wrappers from syscall_wrappers.cpp or similar)
    // These are temporary and reside here for now to make Stream methods callable.
    namespace syscall {
        SyscallResult write_syscall(StreamDescriptor fd, std::span<const std::byte> data) {
            if (!fd.valid()) return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
            // STUB: Actual syscall to write data would go here.
            // For now, simulate successful write of all data.
            // In a real scenario, this would interact with the kernel.
            // e.g., return ::write(static_cast<int>(fd), data.data(), data.size());
            return data.size();
        }

        SyscallResult read_syscall(StreamDescriptor fd, std::span<std::byte> data_buffer) {
            if (!fd.valid()) return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
            // STUB: Actual syscall to read data.
            // For now, simulate EOF.
            // e.g., ssize_t bytes_read = ::read(static_cast<int>(fd), data_buffer.data(), data_buffer.size());
            // if (bytes_read < 0) return std::unexpected(std::make_error_code(std::errc::io_error)); // map errno
            // return static_cast<size_t>(bytes_read);
            return 0;
        }

        std::expected<size_t, std::error_code> seek_syscall(StreamDescriptor fd, std::int64_t offset, int whence) {
            if (!fd.valid()) return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
            // STUB: Actual syscall to seek.
            // e.g., off_t new_pos = ::lseek(static_cast<int>(fd), offset, whence);
            // if (new_pos < 0) return std::unexpected(std::make_error_code(std::errc::invalid_seek)); // map errno
            // return static_cast<size_t>(new_pos);
            return 0; // Simulate seek to beginning or an arbitrary position
        }
    } // namespace syscall


    // Global StreamTable definition
    // This single instance holds all the stream objects.
    static std::array<Stream, max_streams> global_stream_table_instance;

    StreamTable& stream_table() {
        return global_stream_table_instance;
    }

    // Initialization function for the I/O system
    void initialize_io() {
        // Initialize all streams in the global table to a default (closed) state.
        // The Stream default constructor handles making it 'closed'.
        for (size_t i = 0; i < max_streams; ++i) {
             global_stream_table_instance[i] = Stream();
        }

        // Setup standard streams by re-initializing specific slots
        global_stream_table_instance[0] = Stream(StreamDescriptor(0), StreamState::open_read, StreamBufferMode::line);  // stdin
        global_stream_table_instance[1] = Stream(StreamDescriptor(1), StreamState::open_write, StreamBufferMode::line); // stdout
        global_stream_table_instance[2] = Stream(StreamDescriptor(2), StreamState::open_write, StreamBufferMode::none); // stderr
    }

    // Accessors for standard streams
    Stream& standard_input() {
        return global_stream_table_instance[0];
    }
    Stream& standard_output() {
        return global_stream_table_instance[1];
    }
    Stream& standard_error() {
        return global_stream_table_instance[2];
    }


    // Stream method implementations
    Stream::Stream(Stream&& other) noexcept
        : descriptor_(std::exchange(other.descriptor_, StreamDescriptor{})),
          state_(std::exchange(other.state_, StreamState::closed)),
          buffer_mode_(std::exchange(other.buffer_mode_, StreamBufferMode::full)),
          buffer_(other.buffer_), // std::array move assignment (copies PODs like std::byte)
          read_pos_(std::exchange(other.read_pos_, 0)),
          write_pos_(std::exchange(other.write_pos_, 0)) {}

    Stream& Stream::operator=(Stream&& other) noexcept {
        if (this != &other) {
            if (is_open() && needs_flush()) {
                (void)flush_internal(); // Ignore error in assignment move op
            }

            descriptor_ = std::exchange(other.descriptor_, StreamDescriptor{});
            state_ = std::exchange(other.state_, StreamState::closed);
            buffer_mode_ = std::exchange(other.buffer_mode_, StreamBufferMode::full);
            buffer_ = other.buffer_; // std::array move assignment
            read_pos_ = std::exchange(other.read_pos_, 0);
            write_pos_ = std::exchange(other.write_pos_, 0);

            // Ensure the moved-from stream is in a safe, closed state
            other.descriptor_ = StreamDescriptor{};
            other.state_ = StreamState::closed;
            other.read_pos_ = 0;
            other.write_pos_ = 0;
        }
        return *this;
    }

    Stream::~Stream() {
        if (is_open() && needs_flush()) {
            (void)flush_internal(); // Ignore error in destructor
        }
        // If the descriptor_ itself needs closing (e.g. ::close(descriptor_)), it would go here.
        // However, stream objects in the table are persistent; only their state changes.
        // Actual kernel file descriptors are closed by a higher layer (e.g. when process exits or explicitly calls close).
    }

    SyscallResult Stream::flush_internal() noexcept {
        if (write_pos_ == 0) return 0;
        if (!is_open() || (state_ != StreamState::open_write && state_ != StreamState::open_read_write)) {
             // Allow flushing error streams if they have a valid descriptor
            if(state_ == StreamState::error && descriptor_.valid()) {
                // proceed
            } else {
                return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
            }
        }

        auto write_span = std::span<const std::byte>(buffer_.data(), write_pos_);
        auto result = syscall::write_syscall(descriptor_, write_span);

        if (result) {
            size_t actually_written = *result;
            if (actually_written <= write_pos_) { // Can't write more than what's in buffer
                 std::copy(buffer_.data() + actually_written, buffer_.data() + write_pos_, buffer_.data());
                 write_pos_ -= actually_written;
                 if (write_pos_ > 0 && actually_written < write_span.size()) {
                     // Partial write, error state should be set by caller or handled
                     state_ = StreamState::error; // Indicate error due to partial write
                     return std::unexpected(std::make_error_code(std::errc::io_error));
                 }
            } else { // write_syscall reported writing more than available - syscall error
                 state_ = StreamState::error;
                 return std::unexpected(std::make_error_code(std::errc::io_error)); // Should not happen
            }
        } else {
            state_ = StreamState::error;
        }
        return result; // Return original result (could be error or bytes_written)
    }

    SyscallResult Stream::flush() noexcept {
        return flush_internal();
    }

    SyscallResult Stream::write(std::span<const std::byte> data) noexcept {
        if (!is_open() || (state_ != StreamState::open_write && state_ != StreamState::open_read_write)) {
            return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
        }

        size_t total_written_to_stream = 0;
        while (total_written_to_stream < data.size()) {
            if (write_pos_ == buffer_.size()) {
                auto flush_result = flush_internal();
                if (!flush_result) { // Error during flush
                    if (total_written_to_stream > 0) return total_written_to_stream; // Report partial success
                    return flush_result; // Propagate error
                }
            }

            size_t to_copy_to_buffer = std::min(data.size() - total_written_to_stream, buffer_.size() - write_pos_);
            std::memcpy(buffer_.data() + write_pos_, data.data() + total_written_to_stream, to_copy_to_buffer);
            write_pos_ += to_copy_to_buffer;
            total_written_to_stream += to_copy_to_buffer;

            bool should_flush_now = false;
            if (buffer_mode_ == StreamBufferMode::none) {
                should_flush_now = true;
            } else if (buffer_mode_ == StreamBufferMode::line) {
                // Check if the newly added data contains a newline
                const std::byte* newline_char = static_cast<const std::byte*>(memchr(buffer_.data() + write_pos_ - to_copy_to_buffer, '\n', to_copy_to_buffer));
                if (newline_char != nullptr) {
                    should_flush_now = true;
                }
            }
            // Full buffer flush is handled at the start of the loop.

            if (should_flush_now) {
                 auto flush_result = flush_internal();
                 if (!flush_result) { // Error during intermediate flush
                     if (total_written_to_stream > 0) return total_written_to_stream; // Report partial success
                     return flush_result;
                 }
            }
        }
        return total_written_to_stream;
    }

    SyscallResult Stream::fill_internal() noexcept {
        if (!is_open() || (state_ != StreamState::open_read && state_ != StreamState::open_read_write)) {
             return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
        }

        // If there's unread data in the buffer, shift it to the beginning
        if (read_pos_ < write_pos_) { // write_pos_ is end of valid data for reading
            std::copy(buffer_.data() + read_pos_, buffer_.data() + write_pos_, buffer_.data());
            write_pos_ -= read_pos_; // Adjust end of valid data position
        } else { // Buffer entirely consumed or empty
            write_pos_ = 0;
        }
        read_pos_ = 0; // Always read from the start of the buffer

        // How much space is left to fill
        size_t space_to_fill = buffer_.size() - write_pos_;
        if (space_to_fill == 0) { // Buffer is full of already shifted data, no room to read more
            return write_pos_; // Return current amount of data in buffer
        }

        auto read_span = std::span<std::byte>(buffer_.data() + write_pos_, space_to_fill);
        auto result = syscall::read_syscall(descriptor_, read_span);

        if (result) {
            // write_pos_ here tracks the end of valid data in the buffer for reading
            write_pos_ += *result;
            if (*result == 0 && space_to_fill > 0) { // EOF indicated by 0 bytes read
                // Don't change state to closed here, allow consuming existing buffer.
                // The read() method will determine EOF based on repeated zero reads.
            }
        } else {
            state_ = StreamState::error;
        }
        return result; // Returns number of bytes newly read from syscall, or error
    }

    SyscallResult Stream::read(std::span<std::byte> data_buffer) noexcept {
       if (!is_open() || (state_ != StreamState::open_read && state_ != StreamState::open_read_write)) {
           if (state_ == StreamState::closed && read_pos_ < write_pos_) {
               // Stream was closed (e.g. by EOF from fill_internal), but there's still data in buffer
           } else {
               return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
           }
       }

        size_t total_copied_to_user = 0;
        while (total_copied_to_user < data_buffer.size()) {
            if (read_pos_ == write_pos_) { // Buffer is empty or all consumed
                if (state_ == StreamState::closed) break; // EOF reached and buffer empty

                auto fill_result = fill_internal(); // Try to fill buffer
                if (!fill_result) { // Error during fill
                    if (total_copied_to_user > 0) return total_copied_to_user; // Partial success
                    return fill_result;
                }
                if (*fill_result == 0 && write_pos_ == 0) { // True EOF from syscall, buffer is empty
                    state_ = StreamState::closed; // Mark as closed on EOF
                    break;
                }
            }

            // If buffer is still empty after fill attempt (and not true EOF), break.
            if (read_pos_ == write_pos_) break;

            size_t to_copy_from_buffer = std::min(data_buffer.size() - total_copied_to_user, write_pos_ - read_pos_);
            std::memcpy(data_buffer.data() + total_copied_to_user, buffer_.data() + read_pos_, to_copy_from_buffer);
            read_pos_ += to_copy_from_buffer;
            total_copied_to_user += to_copy_from_buffer;
        }
        return total_copied_to_user;
    }

    std::expected<void, std::error_code> Stream::put_char(char c) noexcept {
        std::byte b = static_cast<std::byte>(c);
        auto result = write(std::span<const std::byte>(&b, 1));
        if (!result) return std::unexpected(result.error());
        if (*result != 1) return std::unexpected(std::make_error_code(std::errc::io_error));
        return {};
    }

    std::expected<char, std::error_code> Stream::get_char() noexcept {
        std::byte b;
        auto result = read(std::span<std::byte>(&b, 1));
        if (!result) return std::unexpected(result.error());
        if (*result == 0) { // Proper EOF or error indication from read
             if (state_ == StreamState::closed) { // EOF
                return std::unexpected(std::make_error_code(std::errc::io_error)); // Indicate EOF, could be specific code
             }
             return std::unexpected(std::make_error_code(std::errc::io_error)); // Generic error if not EOF
        }
        return static_cast<char>(b);
    }

    std::expected<size_t, std::error_code> Stream::seek(std::int64_t offset, std::ios_base::seekdir dir) noexcept {
        if (!descriptor_.valid()) {
             return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
        }
        // 1. Flush write buffer if dirty and stream is for writing
        if ((state_ == StreamState::open_write || state_ == StreamState::open_read_write) && write_pos_ > 0) {
            auto flush_res = flush_internal();
            if(!flush_res) return std::unexpected(flush_res.error());
        }
        // 2. Invalidate read buffer (data might change after seek)
        read_pos_ = 0;
        write_pos_ = 0;

        // 3. Perform syscall
        int whence;
        switch(dir) {
            case std::ios_base::beg: whence = 0; break; // SEEK_SET
            case std::ios_base::cur: whence = 1; break; // SEEK_CUR
            case std::ios_base::end: whence = 2; break; // SEEK_END
            default: return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        return syscall::seek_syscall(descriptor_, offset, whence);
    }

    std::expected<size_t, std::error_code> Stream::tell() const noexcept {
        if (!descriptor_.valid()) {
             return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
        }
        // This is complex because of buffering.
        // If write buffer is dirty, the "true" position is current syscall position + write_pos_.
        // If read buffer has unconsumed data, "true" position is current syscall pos - (write_pos_ - read_pos_).
        // For simplicity, this stub might just call lseek(fd, 0, SEEK_CUR) via syscall wrapper.
        // A full implementation needs careful calculation.

        // Call lseek to get current position. If write buffer is dirty, this is problematic.
        // The standard says tell() behavior is undefined if buffer is dirty.
        // So, we could require flush or just return syscall position.
        auto current_pos_syscall = syscall::seek_syscall(descriptor_, 0, 1 /* SEEK_CUR */);
        if(!current_pos_syscall) return std::unexpected(current_pos_syscall.error());

        size_t final_pos = *current_pos_syscall;
        if (state_ == StreamState::open_write || state_ == StreamState::open_read_write) {
            final_pos += write_pos_; // Bytes in write buffer are "past" the syscall's current pos
        } else if (state_ == StreamState::open_read) {
            if (write_pos_ > read_pos_) { // write_pos_ is end of data in read buffer
                 final_pos -= (write_pos_ - read_pos_); // Bytes in read buffer not yet consumed are "before" syscall's EOF on last read
            }
        }
        return final_pos;
    }

} // namespace minix::io
