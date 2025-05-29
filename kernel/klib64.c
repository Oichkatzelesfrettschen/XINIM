#include "const.h"
#include "type.h"
#include "glo.h"
#include "proc.h"
#include <stdint.h>

/* Minimal 64-bit implementations of low level kernel routines. */

/* Copy a block of physical memory. */
void phys_copy(void *src, void *dst, long bytes)
{
    asm volatile(
        "rep movsb"
        : "=S"(src), "=D"(dst), "=c"(bytes)
        : "0"(src), "1"(dst), "2"(bytes)
        : "memory");
}

/* Copy a message from one buffer to another. */
void cp_mess(int src, void *src_click, void *src_off,
             void *dst_click, void *dst_off)
{
    (void)src_click;
    (void)dst_click;
    int *d = dst_off;
    const int *s = src_off;
    d[0] = src;
    asm volatile(
        "lea 4(%[dst]), %%rdi\n\t"
        "lea 4(%[src]), %%rsi\n\t"
        "mov %[count], %%rcx\n\t"
        "rep movsb"
        :
        : [dst] "r"(d), [src] "r"(s), [count] "i"(MESS_SIZE - 4)
        : "rdi", "rsi", "rcx", "memory");
}

/* Output a byte to a port. */
void port_out(unsigned port, unsigned val)
{
    asm volatile("outb %b0, (%w1)" : : "a"(val), "d"(port));
}

/* Input a byte from a port. */
void port_in(unsigned port, unsigned *val)
{
    unsigned char tmp;
    asm volatile("inb (%w1), %b0" : "=a"(tmp) : "d"(port));
    *val = tmp;
}

/* Output a word to a port. */
void portw_out(unsigned port, unsigned val)
{
    asm volatile("outw %w0, (%w1)" : : "a"(val), "d"(port));
}

/* Input a word from a port. */
void portw_in(unsigned port, unsigned *val)
{
    unsigned short tmp;
    asm volatile("inw (%w1), %w0" : "=a"(tmp) : "d"(port));
    *val = tmp;
}

/* Disable interrupts and save flags. */
static uint64_t lockvar;
void lock(void)
{
    asm volatile("pushfq\n\tcli\n\tpop %0" : "=m"(lockvar) :: "memory");
}

/* Enable interrupts. */
void unlock(void)
{
    asm volatile("sti");
}

/* Restore saved interrupt flags. */
void restore(void)
{
    asm volatile("push %0\n\tpopfq" : : "m"(lockvar) : "memory");
}

/* Build a signal frame. */
void build_sig(struct sig_info *dst, struct proc *rp, int sig)
{
    dst->signo = sig;
    dst->sigpcpsw.rip = rp->p_pcpsw.rip;
    dst->sigpcpsw.rflags = rp->p_pcpsw.rflags;
}

/* Get display type (stub returns 0). */
int get_chrome(void)
{
    return 0;
}

/* Copy words to video memory. */
void vid_copy(void *buf, unsigned base, unsigned off, unsigned words)
{
    if (!buf)
        return;
    uint16_t *dst = (uint16_t *)(uintptr_t)(base + off);
    uint16_t *src = buf;
    asm volatile(
        "rep movsw"
        : "=S"(src), "=D"(dst), "=c"(words)
        : "0"(src), "1"(dst), "2"(words)
        : "memory");
}

/* Fetch a byte from memory. */
unsigned char get_byte(unsigned seg, unsigned off)
{
    unsigned char *p = (unsigned char *)(uintptr_t)off;
    (void)seg;
    return *p;
}

/* Halt the CPU. */
void reboot(void)
{
    asm volatile("hlt");
}

/* Halt the CPU (warm reboot placeholder). */
void wreboot(void)
{
    asm volatile("hlt");
}

/* Placeholders for assembly variables from original code. */
uint64_t splimit;

