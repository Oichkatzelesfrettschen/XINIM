/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../h/com.hpp"
#include "../h/type.hpp"
#include "../include/lib.hpp"
#include <unistd.h>

/*
 * Send a message to a destination process.
 */
PUBLIC int send(int dst, message *m_ptr) { return (int)syscall(0, dst, m_ptr, SEND); }

/*
 * Receive a message from a source process.
 */
PUBLIC int receive(int src, message *m_ptr) { return (int)syscall(0, src, m_ptr, RECEIVE); }

/*
 * Atomically send a message and wait for the reply.
 */
PUBLIC int sendrec(int srcdest, message *m_ptr) { return (int)syscall(0, srcdest, m_ptr, BOTH); }
