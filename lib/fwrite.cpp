// Content for lib/fwrite.cpp
#include "../../include/minix/io/stream.hpp" // For minix::io::Stream, Result, IOError
#include <cstddef> // For size_t
#include <cstdio>  // For EOF (though not directly returned by fwrite)
#include <span>    // For std::span, std::as_bytes

// Define EOF if not used from <cstdio> or not in a common header
#ifndef EOF
#define EOF (-1) // fwrite returns count of items, not EOF itself on error
#endif

// fwrite writes 'nmemb' items of 'size' bytes each to 'stream_ptr' from 'ptr'.
// Returns the number of items successfully written.
extern "C" size_t fwrite(const void* ptr, size_t size, size_t nmemb, minix::io::Stream* stream_ptr) {
    if (size == 0 || nmemb == 0) {
        return 0; // As per POSIX, if size or nmemb is 0, fwrite returns 0.
    }
    if (!ptr || !stream_ptr) {
        // TODO: Set errno (POSIX: EINVAL for null ptr if count > 0, EBADF for bad stream)
        return 0;
    }
    if (!stream_ptr->is_open() || !stream_ptr->is_writable()) {
        // TODO: Set errno (EBADF or EPIPE if applicable)
        // TODO: Set error flag on stream if appropriate
        return 0;
    }

    minix::io::Stream& stream = *stream_ptr;

    size_t total_bytes_to_write = size * nmemb;
    // Create a span of const std::byte for the Stream::write method
    std::span<const std::byte> source_buffer(static_cast<const std::byte*>(ptr), total_bytes_to_write);

    auto result = stream.write(source_buffer); // Stream::write returns Result<size_t>

    if (result) {
        // result.value() is the number of bytes successfully written.
        // If this is less than total_bytes_to_write, it indicates a short write (error condition for fwrite).
        // The stream itself should be marked as error in this case by Stream::write or here.
        if (result.value() < total_bytes_to_write) {
            // TODO: Set error flag on the stream object (e.g. stream.set_error_state())
            // TODO: Set errno (e.g., ENOSPC, EIO)
        }
        return result.value() / size; // Number of full items written
    } else {
        // An error occurred during the write operation.
        // The error is in result.error().
        // TODO: Set errno based on result.error().
        // TODO: Set error flag on the stream object. (Stream::write should do this)
        return 0; // On error, fwrite returns 0 items written.
    }
}
