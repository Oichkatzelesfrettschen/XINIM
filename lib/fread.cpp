// Content for lib/fread.cpp
#include "../../include/minix/io/stream.hpp" // For minix::io::Stream, Result, IOError
#include <cstddef> // For size_t
#include <cstdio>  // For EOF (though not directly returned by fread, good for consistency)
#include <span>    // For std::span, std::as_writable_bytes (if needed, but void* is fine)

// Define EOF if not used from <cstdio> or not in a common header
#ifndef EOF
#define EOF (-1) // fread returns count of items, not EOF itself on error/eof
#endif

// fread reads 'nmemb' items of 'size' bytes each from 'stream_ptr' into 'ptr'.
// Returns the number of items successfully read.
extern "C" size_t fread(void* ptr, size_t size, size_t nmemb, minix::io::Stream* stream_ptr) {
    if (size == 0 || nmemb == 0) {
        return 0; // As per POSIX, if size or nmemb is 0, fread returns 0.
    }
    if (!ptr || !stream_ptr) {
        // TODO: Set errno (POSIX: EINVAL for null ptr if count > 0, EBADF for bad stream)
        return 0;
    }
    if (!stream_ptr->is_open() || !stream_ptr->is_readable()) {
        // TODO: Set errno (EBADF)
        // TODO: Set error flag on stream if appropriate (is_readable should check internal state)
        return 0;
    }

    minix::io::Stream& stream = *stream_ptr;

    size_t total_bytes_to_read = size * nmemb;
    // Create a span of std::byte for the Stream::read method
    std::span<std::byte> target_buffer(static_cast<std::byte*>(ptr), total_bytes_to_read);

    auto result = stream.read(target_buffer); // Stream::read returns Result<size_t>

    if (result) {
        // result.value() is the number of bytes successfully read.
        // This can be less than total_bytes_to_read if EOF was encountered.
        return result.value() / size; // Number of full items read
    } else {
        // An error occurred during the read operation (not EOF handled by returning fewer bytes).
        // The error is in result.error().
        // TODO: Set errno based on result.error(). For example:
        // if (result.error() == minix::io::make_error_code(minix::io::IOError::io_error)) { /* set EIO */ }
        // TODO: Set error flag on the stream object itself. (Stream::read should do this)
        return 0; // On error, fread returns 0 items read.
    }
}
