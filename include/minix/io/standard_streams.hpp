#pragma once

#include "stream.hpp" // For minix::io::Stream, StreamPtr, Result, BufferMode
#include "syscall.hpp" // For minix::io::syscall::fd_t

namespace minix::io {

// Maximum number of simultaneously open streams (including stdin, stdout, stderr)
inline constexpr size_t max_open_streams = 20; // Default similar to NFILES

// Accessor functions for standard I/O streams
[[nodiscard]] Stream& get_standard_input() noexcept;
[[nodiscard]] Stream& get_standard_output() noexcept;
[[nodiscard]] Stream& get_standard_error() noexcept;

// Initialization function for the standard streams and stream table
// This should be called once during system startup.
void initialize_standard_streams();

// Function to open a new stream (e.g., for file operations)
// This will find an empty slot in the global table.
// path and posix_flags are for FileStream::open
// Returns a pointer to the new stream, or an error if table is full or open fails.
[[nodiscard]] Result<Stream*> open_new_stream(
    const char* path,
    int posix_flags, // e.g. O_RDONLY, O_CREAT etc from fcntl.h
    int mode_permissions = 0666, // For O_CREAT
    BufferMode buff_mode = BufferMode::full
);

// Function to close and release a stream from the global table.
// Accepts a raw pointer that was previously returned by open_new_stream.
// This is necessary because unique_ptr ownership is in the table.
[[nodiscard]] Result<void> close_managed_stream(Stream* stream_raw_ptr);


// Function to get a stream from the table by its descriptor (if needed for internal use)
// This would require streams to expose their underlying fd_t or for the table to be searchable.
// [[nodiscard]] Stream* get_stream_by_syscall_fd(syscall::fd_t fd) noexcept;

    // Flushes all currently open streams in the global table.
    // Typically called during program exit (_cleanup).
    void flush_all_open_streams() noexcept;

} // namespace minix::io
