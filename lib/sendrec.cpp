#include "../h/com.hpp"
#include "../h/type.hpp"
#include "../include/lib.hpp" // C++17 header
#include <unistd.h>

/*
 * Send a message to a destination process.
 */
// Send a message to a destination process.
int send(int dst, message *m_ptr) { return static_cast<int>(syscall(0, dst, m_ptr, SEND)); }

/*
 * Receive a message from a source process.
 */
// Receive a message from a source process.
int receive(int src, message *m_ptr) { return static_cast<int>(syscall(0, src, m_ptr, RECEIVE)); }

/*
 * Atomically send a message and wait for the reply.
 */
// Atomically send a message and wait for the reply.
int sendrec(int srcdest, message *m_ptr) { return static_cast<int>(syscall(0, srcdest, m_ptr, BOTH)); }
