// Content for lib/getc.cpp
#include "../../include/minix/io/stream.hpp"
#include "../../include/minix/io/standard_streams.hpp" // For get_standard_input
#include <cstdio> // For EOF (or define it in a minix header)

// Define EOF if not available from a common header yet
#ifndef EOF
#define EOF (-1)
#endif

extern "C" int fgetc(minix::io::Stream* stream_ptr) {
    if (!stream_ptr) {
        // TODO: Set errno appropriately (e.g., EBADF)
        return EOF;
    }
    if (!stream_ptr->is_open() || !stream_ptr->is_readable()) {
        // TODO: Set errno (e.g. EBADF)
        // Also, if stream is in error or EOF state already, fgetc should return EOF.
        // The Stream::get_char() itself should handle this by returning an error or specific code.
        return EOF;
    }

    minix::io::Stream& stream = *stream_ptr; // Dereference for use
    auto result = stream.get_char(); // Assuming Stream has get_char() which returns expected<char, error_code>

    if (!result) {
        // Check if the error is EOF
        if (result.error() == minix::io::make_error_code(minix::io::IOError::end_of_file)) {
            // TODO: Set EOF indicator on the stream object itself if not already done by get_char()
        } else {
            // TODO: Set error indicator on the stream / set errno for other errors
        }
        return EOF;
    }
    return static_cast<unsigned char>(result.value()); // Return char as unsigned char converted to int
}

extern "C" int getc(minix::io::Stream* stream_ptr) {
    return fgetc(stream_ptr);
}

extern "C" int getchar() {
    minix::io::Stream& sin = minix::io::get_standard_input();
    // Similar to puts, check if standard_input is usable
    if (!sin.is_open() || !sin.is_readable()) {
        return EOF;
    }
    return fgetc(&sin);
}
