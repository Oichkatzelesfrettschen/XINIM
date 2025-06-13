#pragma once

/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include <unistd.h> // For read, write, ssize_t (assuming these are syscalls)
#include <cstddef>  // For std::size_t, nullptr
#include "../../xinim/core_types.hpp" // For xinim::off_t

// Constants
inline constexpr std::size_t BUFSIZ = 1024;
inline constexpr int NFILES = 20; // Max number of open files
inline constexpr int STDIO_EOF = -1; // End-of-file marker (historically EOF)
// inline constexpr std::nullptr_t NULLPTR = nullptr; // Replaced NULL macro, use nullptr directly

// Flags for _io_buf._flags
inline constexpr int READMODE  = 0x0001; // File open for reading
inline constexpr int WRITEMODE = 0x0002; // File open for writing
inline constexpr int UNBUFF    = 0x0004; // File is unbuffered
inline constexpr int IO_EOF    = 0x0008; // EOF has been reached on this stream (name conflict with STDIO_EOF)
inline constexpr int IO_ERR    = 0x0010; // An error has occurred on this stream
inline constexpr int IOMYBUF   = 0x0020; // _buf was allocated by stdio
inline constexpr int PERPRINTF = 0x0040; // Flush after every printf
inline constexpr int STRINGS   = 0x0080; // Stream is a string (for sprintf/sscanf, if implemented)

// Internal constant
inline constexpr int CMASK = 0377; // Mask for char to int conversion in getc/putc

// FILE structure definition
extern struct _io_buf {
    int _fd;        // File descriptor
    int _count;     // Bytes remaining in buffer (if reading) or count of chars in buffer (if writing)
    int _flags;     // Status flags
    char *_buf;     // Buffer start
    char *_ptr;     // Next char pointer in buffer
} *_io_table[NFILES]; // Table of FILE pointers

// Define FILE as a type alias for struct _io_buf
using FILE = struct _io_buf;

// Standard stream pointers
inline FILE* const stdin_ptr = _io_table[0]; // Using different names to avoid macro clashes
inline FILE* const stdout_ptr = _io_table[1];
inline FILE* const stderr_ptr = _io_table[2];

// Macros for stdin, stdout, stderr for compatibility if needed, but direct use of pointers is cleaner
#define stdin xinim_stdin_ptr  // Remap to avoid issues if <cstdio> is included by other C++ code
#define stdout xinim_stdout_ptr
#define stderr xinim_stderr_ptr
// Make the actual pointers available with a distinct name
extern FILE* xinim_stdin_ptr;
extern FILE* xinim_stdout_ptr;
extern FILE* xinim_stderr_ptr;


// Type for file positioning
using fpos_t = xinim::off_t;

// Constants for fseek 'whence' argument
inline constexpr int SEEK_SET = 0; /* Seek from beginning of file. */
inline constexpr int SEEK_CUR = 1; /* Seek from current position. */
inline constexpr int SEEK_END = 2; /* Seek from end of file. */

// Inline functions replacing historical macros
inline int feof(FILE *p) noexcept { return ((p)->_flags & IO_EOF) != 0; }
inline int ferror(FILE *p) noexcept { return ((p)->_flags & IO_ERR) != 0; }
inline int fileno(FILE *p) noexcept { return (p)->_fd; }
inline void clearerr(FILE *p) noexcept { (p)->_flags &= ~(IO_ERR | IO_EOF); }

// noperprintf and perprintf could be functions if _flags needs more complex logic
inline void noperprintf(FILE *p) noexcept { (p)->_flags &= ~PERPRINTF; }
inline void perprintf(FILE *p) noexcept { (p)->_flags |= PERPRINTF; }

// rewind can be implemented as an inline function
inline void rewind(FILE *stream) noexcept { (void)fseek(stream, 0L, SEEK_SET); clearerr(stream); }

// Function declarations (extern "C" for C library compatibility)
extern "C" {

FILE *fopen(const char *pathname, const char *mode) noexcept;
FILE *freopen(const char *pathname, const char *mode, FILE *stream) noexcept;
int fclose(FILE *stream) noexcept;

std::size_t fread(void *ptr, std::size_t size, std::size_t nmemb, FILE *stream) noexcept;
std::size_t fwrite(const void *ptr, std::size_t size, std::size_t nmemb, FILE *stream) noexcept;

int fgetc(FILE *stream) noexcept; // Often an alias for getc
char *fgets(char *s, int size, FILE *stream) noexcept;
// int getc(FILE *stream) noexcept; // Declaration to be implemented by inline function below

int fputc(int c, FILE *stream) noexcept; // Often an alias for putc
int fputs(const char *s, FILE *stream) noexcept;
// int putc(int c, FILE *stream) noexcept; // Declaration to be implemented by inline function below
int puts(const char *s) noexcept;       // Can be implemented via fputs

int printf(const char *format, ...) noexcept /* TODO: Add format attribute */;
int fprintf(FILE *stream, const char *format, ...) noexcept /* TODO: Add format attribute */;
int sprintf(char *str, const char *format, ...) noexcept /* TODO: Add format attribute */;
// scanf, fscanf, sscanf are more complex due to varargs and return values.
// int scanf(const char *format, ...) noexcept;
// int fscanf(FILE *stream, const char *format, ...) noexcept;
// int sscanf(const char *str, const char *format, ...) noexcept;

int fseek(FILE *stream, xinim::off_t offset, int whence) noexcept;
xinim::off_t ftell(FILE *stream) noexcept;
// rewind is now inline

int fflush(FILE *stream) noexcept;

void perror(const char *s) noexcept;

int remove(const char *pathname) noexcept;
int rename(const char *oldpath, const char *newpath) noexcept;

FILE *tmpfile(void) noexcept;
char *tmpnam(char *s) noexcept; // Note: tmpnam is generally unsafe.

void setbuf(FILE *stream, char *buf) noexcept;
int setvbuf(FILE *stream, char *buf, int mode, std::size_t size) noexcept;

} // extern "C"

// Helper inline function (was missing, needed by getc/putc)
inline bool testflag(FILE *p, int flag) noexcept { return (p->_flags & flag) != 0; }

// Implementations of getc, putc, putchar, getchar

// putchar is a simple inline function
inline int putchar(int c) noexcept {
    // char c_val = static_cast<char>(c); // This was in the prompt, but ::write takes const void*
                                        // so direct use of c after casting to char for safety is fine
                                        // if it's passed by value. For address, variable is needed.
    unsigned char c_val_unsigned = static_cast<unsigned char>(c); // Use unsigned char for consistent behavior
    return ::write(1, &c_val_unsigned, static_cast<std::size_t>(1)) == 1 ? static_cast<int>(c_val_unsigned) : STDIO_EOF;
}

// getc implementation (adapted from original, using updated flag names)
inline int getc(FILE *iop) noexcept {
    char c_unbuf;
    if (testflag(iop, (IO_EOF | IO_ERR)))
        return STDIO_EOF;
    if (!testflag(iop, READMODE)) { // Added error check for not in read mode
        iop->_flags |= IO_ERR; // Set error flag
        return STDIO_EOF;
    }
    if (--iop->_count < 0) {
        if (testflag(iop, UNBUFF)) {
            ssize_t nread = ::read(iop->_fd, &c_unbuf, static_cast<std::size_t>(1));
            if (nread <= 0) {
                iop->_flags |= (nread == 0 ? IO_EOF : IO_ERR);
                return STDIO_EOF;
            }
            iop->_count = 0;
            return static_cast<unsigned char>(c_unbuf); // Ensure positive value before potential implicit CMASK if any
        } else {
            ssize_t nread = ::read(iop->_fd, iop->_buf, BUFSIZ);
            if (nread <= 0) {
                iop->_flags |= (nread == 0 ? IO_EOF : IO_ERR);
                return STDIO_EOF;
            }
            iop->_ptr = iop->_buf;
            iop->_count = static_cast<int>(nread - 1);
        }
    }
    // Ensure the character is treated as unsigned before masking, then cast to int for return.
    return static_cast<int>(static_cast<unsigned char>(*iop->_ptr++) & CMASK);
}

// getchar implementation (inline function replacing macro)
inline int getchar(void) noexcept { // Changed from macro to inline function
    return getc(xinim_stdin_ptr); // Uses the renamed stdin_ptr
}

// putc implementation (adapted from original, using updated flag names)
inline int putc(int ch, FILE *iop) noexcept {
    ssize_t n = 0;
    bool didwrite = false; // Flag to check if a write operation was attempted

    if (testflag(iop, (IO_ERR | IO_EOF))) // Don't write if stream already in error/EOF state
        return STDIO_EOF;
    if (!testflag(iop, WRITEMODE)) { // Cannot write to a stream not opened in write mode
        iop->_flags |= IO_ERR;
        return STDIO_EOF;
    }

    unsigned char char_to_write = static_cast<unsigned char>(ch); // Work with unsigned char

    if (testflag(iop, UNBUFF)) {
        n = ::write(iop->_fd, &char_to_write, static_cast<std::size_t>(1));
        didwrite = true;
    } else {
        // Buffered write
        if (iop->_buf == nullptr) { // Buffer not yet allocated
            // This simple stdio doesn't have auto buffer allocation, assume fopen did it.
            // If not, this is an error condition.
            iop->_flags |= IO_ERR;
            return STDIO_EOF;
        }
        *iop->_ptr++ = char_to_write;
        iop->_count++; // Increment count of characters in buffer

        // Flush buffer if it's full, or if it's a newline and PERPRINTF (line buffering) is on
        if (iop->_count >= static_cast<int>(BUFSIZ) ||
            (testflag(iop, PERPRINTF) && char_to_write == '\n')) {
            if (testflag(iop, STRINGS)) { // Special handling for sprintf-like behavior
                // For string streams, buffer full might mean error or dynamic reallocation (not supported here)
                // Or simply stop writing. For now, let's assume it's an overflow for fixed buffer.
                // This part is underspecified for STRINGS flag.
            } else {
                n = ::write(iop->_fd, iop->_buf, static_cast<std::size_t>(iop->_count));
                didwrite = true;
            }
        }
    }

    if (didwrite) {
        bool write_error = false;
        if (testflag(iop, UNBUFF)) {
            if (n != 1) write_error = true;
        } else { // Buffered write that was flushed
            if (n != iop->_count) write_error = true;
        }

        if (write_error) {
            iop->_flags |= IO_ERR;
            // For buffered output, if write failed, what was in buffer is lost.
            // Resetting count to 0 is appropriate.
            if (!testflag(iop, UNBUFF)) {
                iop->_ptr = iop->_buf; // Reset pointer
                iop->_count = 0;       // No chars successfully written from buffer
            }
            return STDIO_EOF;
        }
        // If buffered write was successful
        if (!testflag(iop, UNBUFF)) {
            iop->_ptr = iop->_buf; // Reset pointer
            iop->_count = 0;       // Buffer is now empty
        }
    }
    return static_cast<int>(char_to_write); // putc returns the character written
}
