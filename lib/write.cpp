#include "../include/lib.hpp" // C++23 header
#include <unistd.h>

/* Perform the write() system call through the message interface. */
ssize_t write(int fd, const void *buffer, size_t nbytes) {
    /* Delegates to the low level message based syscall wrapper. */
    // message m1_i2 (for nbytes) is int.
    // This is a potential narrowing if nbytes (size_t) > INT_MAX.
    // callm1's p1 parameter is char*. Casting const void* to char* requires const_cast.
    return static_cast<ssize_t>(callm1(FS, WRITE, fd, static_cast<int>(nbytes), 0,
                                       const_cast<char *>(reinterpret_cast<const char *>(buffer)),
                                       NIL_PTR, NIL_PTR));
}
