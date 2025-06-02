#pragma once

#include "stream.hpp"    // Base class minix::io::Stream
#include "syscall.hpp" // For minix::io::syscall::fd_t and syscall functions
#include <memory>       // For std::unique_ptr
#include <vector>       // Alternative to unique_ptr<std::byte[]> for buffer if preferred
#include <fcntl.h>      // For O_RDONLY, O_WRONLY, O_RDWR, O_ACCMODE

namespace minix::io {

class FileStream : public Stream {
public:
    // Static factory method to open/create a file and return a FileStream
    [[nodiscard]] static Result<StreamPtr> open(
        const char* path,
        int posix_flags, // e.g., O_RDONLY, O_WRONLY, O_RDWR | O_CREAT | O_APPEND | O_TRUNC
        int mode_permissions = 0666, // Used if O_CREAT is specified
        BufferMode buff_mode = BufferMode::full,
        size_t buffer_size_override = 0 // 0 means use default
    );

    // Constructor is typically private or protected if only factory method is used for creation.
    // Making it public for now if direct instantiation with an existing fd is needed.
    explicit FileStream(
        syscall::fd_t fd,
        int open_flags, // Store the flags used to open (O_RDONLY, O_WRONLY, O_RDWR)
        BufferMode mode = BufferMode::full,
        size_t buffer_size_override = 0
    );

    ~FileStream() override;

    // Move constructor and assignment
    FileStream(FileStream&& other) noexcept;
    FileStream& operator=(FileStream&& other) noexcept;

    // Deleted copy constructor and assignment (inherited from Stream base)
    FileStream(const FileStream&) = delete;
    FileStream& operator=(const FileStream&) = delete;

    // Stream interface overrides
    [[nodiscard]] Result<size_t> read(std::span<std::byte> buffer) override;
    [[nodiscard]] Result<size_t> write(std::span<const std::byte> data) override;
    [[nodiscard]] Result<void> flush() override;
    [[nodiscard]] Result<void> close() override;

    [[nodiscard]] Result<size_t> seek(std::ptrdiff_t offset, SeekDir dir) override;
    [[nodiscard]] Result<size_t> tell() const override;

    [[nodiscard]] State state() const noexcept override;
    [[nodiscard]] bool is_readable() const noexcept override;
    [[nodiscard]] bool is_writable() const noexcept override;

    [[nodiscard]] Result<void> set_buffer_mode(BufferMode mode) override;
    [[nodiscard]] BufferMode buffer_mode() const noexcept override;

private:
    // Helper methods for internal buffer management
    [[nodiscard]] Result<void> internal_fill_buffer() noexcept;
    [[nodiscard]] Result<void> internal_flush_buffer() noexcept;
    void invalidate_read_buffer() noexcept; // Call after a write or seek

    syscall::fd_t fd_{syscall::invalid_fd};
    State current_state_{State::closed};
    BufferMode current_buffer_mode_{BufferMode::full};
    int open_flags_{0}; // Flags used when opening (e.g., O_RDONLY, O_WRONLY, O_RDWR) for is_readable/writable

    std::unique_ptr<std::byte[]> buffer_{nullptr}; // Dynamically allocated buffer
    size_t buffer_size_{0};                        // Actual size of the allocated buffer
    size_t buffer_pos_{0};                         // For reading: current position in the buffer. For writing: end of data to be written.
    size_t buffer_valid_data_len_{0};              // For reading: number of bytes currently valid in the buffer.
    bool buffer_is_dirty_{false};                  // For writing: true if buffer contains data not yet written to fd

    static constexpr size_t default_internal_buffer_size_ = 4096; // Default if not specified
};

} // namespace minix::io
