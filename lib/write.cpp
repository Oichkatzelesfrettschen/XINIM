/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"
#include <unistd.h>

/* Perform the write() system call through the message interface. */
ssize_t write(int fd, const void *buffer, size_t nbytes) {
    /* Delegates to the low level message based syscall wrapper. */
    return (ssize_t)callm1(FS, WRITE, fd, (int)nbytes, 0, (char *)buffer, NIL_PTR, NIL_PTR);
}
