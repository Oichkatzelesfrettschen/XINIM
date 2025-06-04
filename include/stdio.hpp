#pragma once
#include <cstdio>
#include <unistd.h>
/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include <cstdio>

#define BUFSIZ 1024
#define NFILES 20
#define NULL 0
#define EOF (-1)
#define CMASK 0377

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
int fflush(FILE *stream);

#define getchar() getc(stdin)

/* Inline wrapper around std::putchar replacing the previous macro. */
inline int putchar(int c) { return std::putchar(c); }

#define puts(s) fputs(s, stdout)
#define fgetc(f) getc(f)
#define fputc(c, f) putc(c, f)
#define feof(p) (((p)->_flags & _EOF) != 0)
#define ferror(p) (((p)->_flags & _ERR) != 0)
#define fileno(p) ((p)->_fd)
#define rewind(f) fseek(f, 0L, 0)
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
    int ch;
    if (testflag(iop, (_EOF | ERR)))
        return EOF;
    if (!testflag(iop, READMODE))
        return EOF;
    if (--iop->_count <= 0) {
        if (testflag(iop, UNBUFF))
            iop->_count = read(iop->_fd, &ch, 1);
        else
            iop->_count = read(iop->_fd, iop->_buf, BUFSIZ);
        if (iop->_count <= 0) {
            if (iop->_count == 0)
                iop->_flags |= _EOF;
            else
                iop->_flags |= ERR;
            return EOF;
        } else {
            iop->_ptr = iop->_buf;
        }
    }
    if (testflag(iop, UNBUFF))
        return ch & CMASK;
    return *iop->_ptr++ & CMASK;
}

inline int putc(int ch, FILE *iop) {
    int n = 0;
    bool didwrite = false;
    if (testflag(iop, (ERR | _EOF)))
        return EOF;
    if (!testflag(iop, WRITEMODE))
        return EOF;
    if (testflag(iop, UNBUFF)) {
        n = write(iop->_fd, &ch, 1);
        iop->_count = 1;
        didwrite = true;
    } else {
        *iop->_ptr++ = ch;
        if (++iop->_count >= BUFSIZ && !testflag(iop, STRINGS)) {
            n = write(iop->_fd, iop->_buf, iop->_count);
            iop->_ptr = iop->_buf;
            didwrite = true;
        }
    }
    if (didwrite) {
        if (n <= 0 || iop->_count != n) {
            if (n < 0)
                iop->_flags |= ERR;
            else
                iop->_flags |= _EOF;
            return EOF;
        }
        iop->_count = 0;
    }
    return 0;
}
