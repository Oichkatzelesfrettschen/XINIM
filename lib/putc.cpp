// Content for lib/putc.cpp
#include "../../include/minix/io/stream.hpp"
#include "../../include/minix/io/standard_streams.hpp" // For get_standard_output
#include <cstdio> // For EOF (or define it in a minix header)

// Define EOF if not available from a common header yet
#ifndef EOF
#define EOF (-1)
#endif

extern "C" int fputc(int c, minix::io::Stream* stream_ptr) {
    if (!stream_ptr) {
        // TODO: Set errno appropriately (e.g., EBADF)
        return EOF;
    }
    if (!stream_ptr->is_open() || !stream_ptr->is_writable()) {
        // TODO: Set errno (e.g. EBADF)
        return EOF;
    }

    minix::io::Stream& stream = *stream_ptr; // Dereference for use
    auto result = stream.put_char(static_cast<char>(c)); // Assuming Stream has put_char()

    if (!result) {
        // TODO: Set error flag on stream and errno
        return EOF;
    }
    return static_cast<unsigned char>(c); // Success, return the character written
}

extern "C" int putc(int c, minix::io::Stream* stream_ptr) {
    return fputc(c, stream_ptr);
}

extern "C" int putchar(int c) {
    minix::io::Stream& sout = minix::io::get_standard_output();
    if (!sout.is_open() || !sout.is_writable()) {
        return EOF;
    }
    return fputc(c, &sout);
}
