#include "../include/lib.hpp" // C++23 header
#include <stddef.h>           // For size_t
#include <sys/types.h>        // For ssize_t

// Read up to 'nbytes' from file descriptor 'fd' into 'buffer'.
// POSIX: ssize_t read(int fd, void *buf, size_t count);
ssize_t read(int fd, void *buffer, size_t nbytes) { // Changed signature
    // message m1_i2 (for nbytes) is int.
    // This is a potential narrowing if nbytes (size_t) > INT_MAX.
    // This reflects a limitation of the underlying syscall message format.
    int n = callm1(FS, READ, fd, static_cast<int>(nbytes), 0,
                   static_cast<char *>(buffer), // cast void* to char* for callm1
                   NIL_PTR, NIL_PTR);
    return static_cast<ssize_t>(n); // Cast int return to ssize_t
}
