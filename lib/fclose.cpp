#include "../include/lib.hpp" // C++23 header
#include "../include/stdio.hpp"
#include <unistd.h>

/**
 * @file fclose.cpp
 * @brief Modern C++23 implementation of the standard `fclose` function.
 *
 * This function provides a robust and thread-safe way to close an I/O stream.
 * It first flushes any buffered data, then closes the underlying file descriptor,
 * and finally releases the stream's resources.
 *
 * @ingroup utility
 */

/**
 * @brief Close a stream and release its resources.
 *
 * @param fp Pointer to the FILE structure to close.
 * @return 0 on success or `STDIO_EOF` on failure.
 * @sideeffects Flushes and closes the underlying file descriptor, and frees the buffer.
 * @thread_safety The caller must ensure a single thread is accessing the `FILE` object.
 * @compat fclose(3)
 * @example
 * fclose(file);
 */
int fclose(FILE *fp) {
    int i; // index within _io_table

    // Find the stream in the global table and remove it.
    for (i = 0; i < NFILES; i++) {
        if (fp == _io_table[i]) {
            _io_table[i] = nullptr;
            break;
        }
    }

    if (i >= NFILES) {
        // Stream not found, it might have been closed already or is invalid.
        return STDIO_EOF;
    }

    // Attempt to flush the stream before closing.
    fflush(fp);
    
    // Close the underlying file descriptor.
    close(fp->_fd);

    // Free the buffer if it was dynamically allocated.
    if (testflag(fp, IOMYBUF) && fp->_buf) {
        safe_free(fp->_buf);
    }

    // Finally, free the FILE structure itself.
    safe_free(fp);

    return 0;
}