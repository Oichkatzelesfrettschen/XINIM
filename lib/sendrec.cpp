#include "../h/com.hpp"
#include "../h/type.hpp"
#include "../include/lib.hpp" // C++17 header
#include <unistd.h>

/**
 * @brief Send a message to a destination process.
 *
 * @param dst  Destination process identifier.
 * @param m_ptr Pointer to the message structure.
 * @return Result of the system call.
 */
int send(int dst, message *m_ptr) noexcept {
    return static_cast<int>(syscall(0, dst, m_ptr, SEND));
}

/**
 * @brief Receive a message from a source process.
 *
 * @param src   Source process identifier.
 * @param m_ptr Pointer to the message structure.
 * @return Result of the system call.
 */
int receive(int src, message *m_ptr) noexcept {
    return static_cast<int>(syscall(0, src, m_ptr, RECEIVE));
}

/**
 * @brief Atomically send a message and wait for the reply.
 *
 * @param srcdest Destination/source process identifier.
 * @param m_ptr   Pointer to the message structure.
 * @return Result of the system call.
 */
int sendrec(int srcdest, message *m_ptr) noexcept {
    return static_cast<int>(syscall(0, srcdest, m_ptr, BOTH));
}
