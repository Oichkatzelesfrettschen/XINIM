#pragma once
#include <cstddef>  // For std::size_t, nullptr
#include <unistd.h> // For read, write, ssize_t

/**
 * @file stdio.hpp
 * @brief Modern C++23 standard I/O library header.
 *
 * This header provides a modernized, C++23-compliant implementation of a simple
 * standard I/O library. It is designed to replace the legacy `stdio`
 * with a more robust, type-safe, and well-documented version while
 * maintaining API compatibility.
 *
 * The implementation uses `inline constexpr` for compile-time constants,
 * strong types, and inline functions to replace traditional C macros,
 * thereby improving safety and performance.
 *
 * @ingroup utility
 */

inline constexpr std::size_t BUFSIZ = 1024;
inline constexpr int NFILES = 20;
inline constexpr int STDIO_EOF = -1;
inline constexpr int CMASK = 0377; // Mask for character classification

// Status flags for the file stream.
constexpr int READMODE = 1;
constexpr int WRITEMODE = 2;
constexpr int UNBUFF = 4;
constexpr int _EOF = 8;
constexpr int ERR = 16;
constexpr int _ERR = ERR;
constexpr int IOMYBUF = 32;
constexpr int PERPRINTF = 64;
constexpr int STRINGS = 128;

#ifndef FILE
/**
 * @brief Simple I/O stream buffer structure.
 *
 * This structure holds the state for a single I/O stream, including the
 * file descriptor, buffer pointer, and status flags. It is designed to be
 * compatible with C-style libraries that use this structure.
 */
extern struct _io_buf {
    int _fd;    /** file descriptor */
    int _count; /** bytes remaining in buffer */
    int _flags; /** status flags */
    char *_buf; /** buffer start */
    char *_ptr; /** next char in buffer */
} *_io_table[NFILES];

#endif /* FILE */

/** @typedef FILE
 * @brief Alias for the stream buffer structure.
 */
#define FILE struct _io_buf

/** @var stdin
 * @brief The standard input stream.
 */
#define stdin (_io_table[0])
/** @var stdout
 * @brief The standard output stream.
 */
#define stdout (_io_table[1])
/** @var stderr
 * @brief The standard error stream.
 */
#define stderr (_io_table[2])

/**
 * @brief Flushes the buffer associated with the given stream.
 * @param stream The stream to flush.
 * @return 0 on success, EOF on failure.
 */
extern "C" int fflush(FILE *stream);
/**
 * @brief Writes a string to a stream.
 * @param s The null-terminated string to write.
 * @param stream The destination stream.
 * @return 0 on success, EOF on failure.
 */
extern "C" int xinim_fputs(const char *s, FILE *stream);
/**
 * @brief Prints formatted text to stdout.
 * @param fmt The format string.
 * @return The number of characters printed.
 */
extern "C" int printf(const char *fmt, ...);

/**
 * @brief Reads a character from standard input.
 *
 * This is an inline wrapper that provides a C-style macro for convenience.
 * @return The character read, or EOF on end-of-file or error.
 */
#define xinim_getchar() getc(stdin)

/**
 * @brief Writes a character to standard output.
 *
 * This function provides a modern, type-safe implementation of putchar.
 * It uses the low-level `write` system call.
 *
 * @param c The character to write.
 * @return The character written, or EOF on failure.
 */
inline int putchar(int c) {
    return ::write(STDOUT_FILENO, &c, static_cast<size_t>(1)) == 1 ? c : STDIO_EOF;
}

/**
 * @brief Writes a string to stdout.
 * @param s The string to write.
 * @return 0 on success, EOF on failure.
 */
#define puts(s) fputs(s, stdout)
/**
 * @brief Reads a character from a stream.
 * @param f The stream to read from.
 * @return The character read, or EOF on end-of-file or error.
 */
#define fgetc(f) getc(f)
/**
 * @brief Writes a character to a stream.
 * @param c The character to write.
 * @param f The stream to write to.
 * @return The character written, or EOF on failure.
 */
#define fputc(c, f) putc(c, f)
/**
 * @brief Checks for end-of-file.
 * @param p The stream to check.
 * @return Non-zero if end-of-file is set, 0 otherwise.
 */
#define feof(p) (((p)->_flags & _EOF) != 0)
/**
 * @brief Checks for an error on a stream.
 * @param p The stream to check.
 * @return Non-zero if an error is set, 0 otherwise.
 */
#define ferror(p) (((p)->_flags & _ERR) != 0)
/**
 * @brief Gets the file descriptor for a stream.
 * @param p The stream.
 * @return The file descriptor.
 */
#define fileno(p) ((p)->_fd)
/**
 * @brief Rewinds a stream to the beginning.
 * @param f The stream to rewind.
 */
#define rewind(f) fseek(f, 0L, 0)
/**
 * @brief Checks for a flag on a stream.
 * @param p The stream.
 * @param x The flag to check.
 * @return Non-zero if the flag is set, 0 otherwise.
 */
#define testflag(p, x) ((p)->_flags & (x))

/* If you want a stream to be flushed after each printf use:
 *
 * perprintf(stream);
 *
 * If you want to stop with this kind of buffering use:
 *
 * noperprintf(stream);
 */

/**
 * @brief Disables per-printf flushing for a stream.
 * @param p The stream.
 */
#define noperprintf(p) ((p)->_flags &= ~PERPRINTF)
/**
 * @brief Enables per-printf flushing for a stream.
 * @param p The stream.
 */
#define perprintf(p) ((p)->_flags |= PERPRINTF)

/**
 * @brief Reads a character from a buffered stream.
 *
 * This function provides a modern, type-safe implementation of `getc`.
 * It handles buffered and unbuffered reads, and correctly sets status flags.
 *
 * @param iop The stream to read from.
 * @return The character read, or `STDIO_EOF` on failure or end-of-file.
 */
inline int getc(FILE *iop) {
    char c_unbuf; // Temporary for unbuffered read character
    if (testflag(iop, (_EOF | ERR)))
        return STDIO_EOF;
    if (!testflag(iop, READMODE))
        return STDIO_EOF;
    if (--iop->_count < 0) { // Buffer is empty or was initially zero (for unbuffered)
        if (testflag(iop, UNBUFF)) {
            ssize_t nread = ::read(iop->_fd, &c_unbuf, static_cast<size_t>(1));
            if (nread <= 0) {
                iop->_flags |= (nread == 0 ? _EOF : ERR);
                return STDIO_EOF;
            }
            iop->_count = 0;
            return c_unbuf & CMASK;
        } else {
            ssize_t nread = ::read(iop->_fd, iop->_buf, BUFSIZ); // BUFSIZ is std::size_t
            if (nread <= 0) {
                iop->_flags |= (nread == 0 ? _EOF : ERR);
                return STDIO_EOF;
            }
            iop->_ptr = iop->_buf;
            iop->_count = static_cast<int>(nread - 1); // One char is about to be consumed
        }
    }
    return *iop->_ptr++ & CMASK;
}

/**
 * @brief Writes a character to a buffered stream.
 *
 * This function provides a modern, type-safe implementation of `putc`.
 * It handles buffered and unbuffered writes, and correctly sets status flags.
 *
 * @param ch  The character to write.
 * @param iop The stream to write to.
 * @return The character written, or `STDIO_EOF` on failure.
 */
inline int putc(int ch, FILE *iop) {
    ssize_t n = 0; // To match return type of write
    bool didwrite = false;
    if (testflag(iop, (ERR | _EOF)))
        return STDIO_EOF;
    if (!testflag(iop, WRITEMODE))
        return STDIO_EOF;
    if (testflag(iop, UNBUFF)) {
        char c_val = static_cast<char>(ch);
        n = ::write(iop->_fd, &c_val, static_cast<size_t>(1));
        didwrite = true;
    } else {
        *iop->_ptr++ = static_cast<char>(ch);
        if (++iop->_count >= static_cast<int>(BUFSIZ) &&
            !testflag(iop, STRINGS)) { // BUFSIZ is std::size_t
            n = ::write(iop->_fd, iop->_buf, static_cast<size_t>(iop->_count));
            iop->_ptr = iop->_buf;
            didwrite = true;
        }
    }
    if (didwrite) {
        if (n <= 0 || iop->_count != static_cast<int>(n)) { // n is ssize_t, iop->_count is int
            if (n < 0)
                iop->_flags |= ERR;
            else
                iop->_flags |= _EOF; // Original code set _EOF for short write too.
            return STDIO_EOF;
        }
        iop->_count = 0;
    }
    return ch & CMASK; // putc returns the character written on success
}