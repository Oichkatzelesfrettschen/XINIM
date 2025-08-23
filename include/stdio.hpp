#pragma once
#include <cstddef>  // For std::size_t, nullptr
#include <unistd.h> // For read, write, ssize_t
// #include "../../xinim/core_types.hpp" // Not directly using xinim types here

/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

inline constexpr std::size_t BUFSIZ = 1024;
inline constexpr int NFILES = 20;
// MODERNIZED: #define NULL 0 // Use nullptr in C++ code
inline constexpr int STDIO_EOF = -1; // Renamed from EOF
inline constexpr int CMASK = 0377;

constexpr int READMODE = 1;
constexpr int WRITEMODE = 2;
constexpr int UNBUFF = 4;
constexpr int _EOF = 8;
constexpr int ERR = 16;
constexpr int _ERR = ERR; // compatibility with historical macro
constexpr int IOMYBUF = 32;
constexpr int PERPRINTF = 64;
constexpr int STRINGS = 128;

#ifndef FILE
// Table of file streams managed by the simple stdio layer
//

extern struct _io_buf {
    int _fd;    /** file descriptor */
    int _count; /** bytes remaining in buffer */
    int _flags; /** status flags */
    char *_buf; /** buffer start */
    char *_ptr; /** next char in buffer */
} *_io_table[NFILES];

#endif /* FILE */

#define FILE struct _io_buf

#define stdin (_io_table[0])
#define stdout (_io_table[1])
#define stderr (_io_table[2])

// Flush the buffer associated with the given stream.
extern "C" int fflush(FILE *stream);
extern "C" int xinim_fputs(const char *s, FILE *stream);
/**
 * @brief Print formatted text to @c stdout.
 */
extern "C" int printf(const char *fmt, ...);

#define xinim_getchar() getc(stdin)

/* Write a character to stdout using the low-level write call. */
inline int putchar(int c) {
    // ::write's 3rd param is size_t.
    return ::write(1, &c, static_cast<size_t>(1)) == 1 ? c : STDIO_EOF;
}

#define puts(s) fputs(s, stdout)
#define fgetc(f) getc(f)
#define fputc(c, f) putc(c, f)
#define feof(p) (((p)->_flags & _EOF) != 0)
#define ferror(p) (((p)->_flags & _ERR) != 0)
#define fileno(p) ((p)->_fd)
#define rewind(f) fseek(f, 0L, 0) // fseek takes long for offset
#define testflag(p, x) ((p)->_flags & (x))

/* If you want a stream to be flushed after each printf use:
 *
 *	perprintf(stream);
 *
 * If you want to stop with this kind of buffering use:
 *
 *	noperprintf(stream);
 */

#define noperprintf(p) ((p)->_flags &= ~PERPRINTF)
#define perprintf(p) ((p)->_flags |= PERPRINTF)

// Inline wrappers replacing historical getc/putc macros
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
        // iop->_count = 1; // This was original logic, seems incorrect for unbuffered putc.
        // _count usually refers to buffered chars.
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
            else                     // This case (n > 0 but n != iop->_count) is a short write.
                iop->_flags |= _EOF; // Original code set _EOF for short write too.
            return STDIO_EOF;
        }
        iop->_count = 0;
    }
    return ch & CMASK; // putc returns the character written on success
}
