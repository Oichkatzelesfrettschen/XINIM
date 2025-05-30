#include "../include/lib.h"
#include <unistd.h>

/* Perform the write() system call through the message interface. */
ssize_t write(int fd, const void *buffer, size_t nbytes) {
    /* Delegates to the low level message based syscall wrapper. */
    return (ssize_t)callm1(FS, WRITE, fd, (int)nbytes, 0, (char *)buffer, NIL_PTR, NIL_PTR);
}
