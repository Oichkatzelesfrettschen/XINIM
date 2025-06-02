// Content for lib/fputs.cpp
#include "../../include/minix/io/stream.hpp" // For minix::io::Stream
#include "../../include/minix/io/standard_streams.hpp" // For get_standard_output as default
#include <cstring> // For std::strlen
#include <cstdio>  // For EOF (or define it in a minix header)
#include <span>    // For std::span, std::as_bytes

// Define EOF if not available from a common header yet
#ifndef EOF
#define EOF (-1)
#endif

extern "C" int fputs(const char* s, minix::io::Stream* stream_ptr) {
    if (!s || !stream_ptr) {
        // TODO: Set errno appropriately (e.g., EFAULT or EINVAL for null s, EBADF for null stream_ptr)
        return EOF;
    }
    if (!stream_ptr->is_open() || !stream_ptr->is_writable()) {
        // TODO: Set errno appropriately (e.g., EBADF)
        return EOF;
    }

    minix::io::Stream& stream = *stream_ptr; // Dereference for use
    size_t len = std::strlen(s);
    if (len == 0) {
        return 0; // Success, nothing to write
    }
    auto result = stream.write(std::as_bytes(std::span(s, len)));
    if (!result || result.value() != len) {
        // TODO: Set error flag on stream and errno
        return EOF;
    }
    return 0; // Success, return non-negative
}

extern "C" int puts(const char* s) {
    minix::io::Stream& sout = minix::io::get_standard_output();
    // Check if standard_output itself is valid before using it
    if (!sout.is_open() || !sout.is_writable()) {
         // This case is tricky: where to report error if stderr itself might be broken?
         // For now, return EOF. A robust system might try a raw syscall for stderr.
        return EOF;
    }

    int result = fputs(s, &sout);
    if (result == EOF) {
        return EOF;
    }

    char newline = '\n';
    auto nl_res = sout.write(std::as_bytes(std::span(&newline, 1)));
    if (!nl_res || nl_res.value() != 1) {
        // TODO: Set error flag on stream and errno
        return EOF;
    }
    // Standard puts returns non-negative on success.
    // fputs returns 0 on success, EOF on error.
    // So, if fputs succeeded (0) and newline write succeeded, return 0 (or any non-negative).
    return 0;
}
