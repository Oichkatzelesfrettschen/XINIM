#ifdef __x86_64__
#include "../h/com.h"
#include "../h/type.h"

PUBLIC int send(int dst, message *m_ptr)
{
    register long rax asm("rax") = 0;
    register long rdi asm("rdi") = dst;
    register message *rsi asm("rsi") = m_ptr;
    register long rdx asm("rdx") = SEND;
    asm volatile ("syscall" : "=a"(rax) : "D"(rdi), "S"(rsi), "d"(rdx), "a"(rax) : "rcx", "r11", "memory");
    return (int)rax;
}

PUBLIC int receive(int src, message *m_ptr)
{
    register long rax asm("rax") = 0;
    register long rdi asm("rdi") = src;
    register message *rsi asm("rsi") = m_ptr;
    register long rdx asm("rdx") = RECEIVE;
    asm volatile ("syscall" : "=a"(rax) : "D"(rdi), "S"(rsi), "d"(rdx), "a"(rax) : "rcx", "r11", "memory");
    return (int)rax;
}

PUBLIC int sendrec(int srcdest, message *m_ptr)
{
    register long rax asm("rax") = 0;
    register long rdi asm("rdi") = srcdest;
    register message *rsi asm("rsi") = m_ptr;
    register long rdx asm("rdx") = BOTH;
    asm volatile ("syscall" : "=a"(rax) : "D"(rdi), "S"(rsi), "d"(rdx), "a"(rax) : "rcx", "r11", "memory");
    return (int)rax;
}
#endif

