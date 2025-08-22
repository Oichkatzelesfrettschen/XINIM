#ifdef __x86_64__
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/type.hpp"

/**
 * @brief x86_64 syscall wrapper for sending a message.
 *
 * @param dst   Destination process identifier.
 * @param m_ptr Pointer to message structure.
 * @return Result of the system call.
 * @sideeffects Performs a kernel trap to send a message.
 * @thread_safety Thread-safe; relies on kernel-level isolation.
 */
int send(int dst, message *m_ptr) noexcept {
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
 * @param src   Source process identifier.
 * @param m_ptr Pointer to message structure.
 * @return Result of the system call.
 * @sideeffects Blocks until a message arrives.
 * @thread_safety Thread-safe; relies on kernel-level isolation.
 */
int receive(int src, message *m_ptr) noexcept {
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
 * @brief x86_64 syscall wrapper for sending a message and waiting for reply.
 *
 * @param srcdest Destination/source identifier.
 * @param m_ptr   Pointer to message structure.
 * @return Result of the system call.
 * @sideeffects Traps into the kernel and may block for a reply.
 * @thread_safety Thread-safe; relies on kernel-level isolation.
 */
int sendrec(int srcdest, message *m_ptr) noexcept {
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
