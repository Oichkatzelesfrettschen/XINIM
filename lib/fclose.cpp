#include "../include/lib.hpp" // C++23 header
#include "../include/stdio.hpp"
#include <unistd.h>

/**
 * @brief Close a stream and release its resources.
 *
 * @param fp Pointer to the FILE structure to close.
 * @return 0 on success or EOF on failure.
 * @sideeffects Flushes and closes the underlying file descriptor.
 * @thread_safety Not thread-safe on shared FILE objects.
 * @compat fclose(3)
 * @example
 * fclose(file);
 */
int fclose(FILE *fp) {
    int i; /* index within _io_table */

    for (i = 0; i < NFILES; i++) {
        if (fp == _io_table[i]) {
            _io_table[i] = 0;
            break;
        }
    }
    if (i >= NFILES)
        return STDIO_EOF;
    fflush(fp);
    close(fp->_fd);
    if (testflag(fp, IOMYBUF) && fp->_buf)
        safe_free(fp->_buf);
    safe_free(fp);
    return 0;
}
