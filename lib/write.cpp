/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header
#include <unistd.h>

/* Perform the write() system call through the message interface. */
ssize_t write(int fd, const void *buffer, size_t nbytes) {
    /* Delegates to the low level message based syscall wrapper. */
    return (ssize_t)callm1(FS, WRITE, fd, (int)nbytes, 0, (char *)buffer, NIL_PTR, NIL_PTR);
}
