// Content for lib/fgets.cpp
#include "../../include/minix/io/stream.hpp" // For minix::io::Stream, Result, IOError
#include <cstddef>  // For size_t, nullptr_t
#include <cstdio>   // For EOF (or define it in a minix header)

// Define EOF if not available from a common header yet
#ifndef EOF
#define EOF (-1) // fgets returns char*, not EOF directly, but uses NULL for error/EOF conditions
#endif

// fgets reads at most n-1 characters from stream_ptr into s.
// Reading stops after an EOF or a newline. If a newline is read,
// it is stored into the buffer. A terminating null byte ('\0') is stored
// after the last character in the buffer.
// Returns s on success, and NULL on error or when end of file occurs
// while no characters have been read.
extern "C" char* fgets(char* s, int n, minix::io::Stream* stream_ptr) {
    if (!s || n <= 0) { // Invalid arguments for s or n
        // TODO: Set errno (EINVAL)
        return nullptr;
    }
    if (!stream_ptr) { // Null stream pointer
        // TODO: Set errno (EBADF or other appropriate error)
        if (n > 0) s[0] = '\0'; // Ensure null termination if s is valid
        return nullptr;
    }
    if (!stream_ptr->is_open() || !stream_ptr->is_readable()) {
        // TODO: Set errno (e.g., EBADF)
        if (n > 0) s[0] = '\0'; // Ensure null termination
        return nullptr;
    }

    minix::io::Stream& stream = *stream_ptr;
    int i = 0;
    char c = 0;

    if (n == 1) { // Only space for null terminator
        s[0] = '\0';
        return s;
    }

    while (i < n - 1) { // Leave space for null terminator
        auto char_result = stream.get_char(); // Assuming Stream has get_char()

        if (!char_result) {
            // Error or EOF from get_char()
            if (i == 0) { // No characters read before error/EOF
                // Whether it's an error or EOF without reading any char, fgets returns NULL.
                // TODO: If char_result.error() is not IOError::end_of_file, set errno.
                s[i] = '\0'; // Ensure buffer is null-terminated.
                return nullptr;
            }
            // EOF or error occurred after some characters were read.
            // The loop will break, string will be terminated, and s will be returned.
            break;
        }

        c = char_result.value();
        s[i++] = c;

        if (c == '\n') {
            break; // Newline read, stored, now stop.
        }
    }

    s[i] = '\0'; // Null-terminate the string

    // If i is 0 here, it means either:
    // 1. n was 1 (handled at the start).
    // 2. The loop broke immediately on the first get_char() AND i was 0 (this case returns nullptr above).
    // So if we reach here and i is 0, it must have been n=1.
    // If i > 0, it means at least one character was successfully read (or n=1 and s[0]='\0').
    // The conditions for returning NULL (error, or EOF before any char read) are handled inside the loop.
    // Thus, if we exit the loop and haven't returned NULL, it's a success.
    return s;
}
