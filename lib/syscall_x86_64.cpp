#ifdef __x86_64__
#include "../h/com.h"
#include "../h/const.h"
#include "../h/type.h"

PUBLIC int send(int dst, message *m_ptr) {
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

PUBLIC int receive(int src, message *m_ptr) {
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

PUBLIC int sendrec(int srcdest, message *m_ptr) {
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
