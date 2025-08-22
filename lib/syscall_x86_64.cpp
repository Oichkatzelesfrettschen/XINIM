#ifdef __x86_64__
/**
 * @file syscall_x86_64.cpp
 * @brief User-space syscall wrappers for x86_64.
 */
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/type.hpp"

/**
 * @brief x86_64 syscall wrapper for sending a message.
 *
 * This function traps into the kernel to send a message to a destination process.
 * The destination ID and message pointer are passed via registers,
 * and the system call number is loaded into `rdx`.
 *
 * @param dst   Destination process identifier.
 * @param m_ptr Pointer to the message structure to be sent.
 * @return The result of the system call (typically ::OK or an error code).
 * @sideeffects Traps into the kernel to perform a message send operation.
 * @thread_safety This function is thread-safe as it relies on kernel-level isolation
 * for message passing.
 * @ingroup syscall
 */
[[nodiscard]] int send(int dst, message *m_ptr) noexcept {
    register long rax __asm__("rax") = 0;
    register long rdi __asm__("rdi") = dst;
    register message *rsi __asm__("rsi") = m_ptr;
    register long rdx __asm__("rdx") = SEND;
    __asm__ volatile("syscall"
                     : "=a"(rax)
                     : "D"(rdi), "S"(rsi), "d"(rdx), "a"(rax)
                     : "rcx", "r11", "memory");
    return (int)rax;
}

/**
 * @brief x86_64 syscall wrapper for receiving a message.
 *
 * This function traps into the kernel to receive a message. It blocks
 * until a message is available from the specified source. The source ID
 * and message buffer are passed via registers, and the system call number
 * is loaded into `rdx`.
 *
 * @param src   Source process identifier.
 * @param m_ptr Pointer to the message structure to receive the message into.
 * @return The result of the system call.
 * @sideeffects Blocks until a message arrives and modifies the message buffer.
 * @thread_safety This function is thread-safe as it relies on kernel-level
 * isolation and synchronization for message passing.
 * @ingroup syscall
 */
[[nodiscard]] int receive(int src, message *m_ptr) noexcept {
    register long rax __asm__("rax") = 0;
    register long rdi __asm__("rdi") = src;
    register message *rsi __asm__("rsi") = m_ptr;
    register long rdx __asm__("rdx") = RECEIVE;
    __asm__ volatile("syscall"
                     : "=a"(rax)
                     : "D"(rdi), "S"(rsi), "d"(rdx), "a"(rax)
                     : "rcx", "r11", "memory");
    return (int)rax;
}

/**
 * @brief x86_64 syscall wrapper for sending a message and waiting for a reply.
 *
 * This function is a synchronous wrapper for `send` and `receive`. It sends a message
 * to a destination process and then immediately blocks, waiting for a reply from
 * that same process. The destination/source ID and message buffer are passed
 * via registers.
 *
 * @param srcdest Destination/source identifier.
 * @param m_ptr   Pointer to the message structure.
 * @return The result of the system call.
 * @sideeffects Traps into the kernel and may block for a reply.
 * @thread_safety This function is thread-safe as it relies on kernel-level
 * isolation and synchronization for message passing.
 * @ingroup syscall
 */
[[nodiscard]] int sendrec(int srcdest, message *m_ptr) noexcept {
    register long rax __asm__("rax") = 0;
    register long rdi __asm__("rdi") = srcdest;
    register message *rsi __asm__("rsi") = m_ptr;
    register long rdx __asm__("rdx") = BOTH;
    __asm__ volatile("syscall"
                     : "=a"(rax)
                     : "D"(rdi), "S"(rsi), "d"(rdx), "a"(rax)
                     : "rcx", "r11", "memory");
    return (int)rax;
}
#endif