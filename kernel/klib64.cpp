#include "../h/const.hpp"
#include "../h/type.hpp"
#include "../include/defs.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"

/* Minimal 64-bit implementations of low level kernel routines. */

/*===========================================================================*
 *                              phys_copy                                    *
 *===========================================================================*/
/* Copy a block of physical memory.
 * src:  source address.
 * dst:  destination address.
 * bytes: number of bytes to copy.
 */
void phys_copy(void *src, void *dst, long bytes) {
    asm volatile("rep movsb"
                 : "=S"(src), "=D"(dst), "=c"(bytes)
                 : "0"(src), "1"(dst), "2"(bytes)
                 : "memory");
}

/*===========================================================================*
 *                              cp_mess                                      *
 *===========================================================================*/
/* Copy a message from one buffer to another.
 * src: source process number.
 * src_click, src_off: unused address parts.
 * dst_click, dst_off: destination buffer.
 */
void cp_mess(int src, void *src_click, void *src_off, void *dst_click, void *dst_off) {
    (void)src_click;
    (void)dst_click;
    int *d = dst_off;
    const int *s = src_off;
    d[0] = src;
    asm volatile("lea 4(%[dst]), %%rdi\n\t"
                 "lea 4(%[src]), %%rsi\n\t"
                 "mov %[count], %%rcx\n\t"
                 "rep movsb"
                 :
                 : [dst] "r"(d), [src] "r"(s), [count] "i"(MESS_SIZE - 4)
                 : "rdi", "rsi", "rcx", "memory");
}

/*===========================================================================*
 *                              port_out                                     *
 *===========================================================================*/
/* Output a byte to an I/O port.
 * port: port number.
 * val: value to output.
 */
void port_out(unsigned port, unsigned val) {
    asm volatile("outb %b0, (%w1)" : : "a"(val), "d"(port));
}

/*===========================================================================*
 *                              port_in                                      *
 *===========================================================================*/
/* Input a byte from an I/O port.
 * port: port number.
 * val: location to store the read byte.
 */
void port_in(unsigned port, unsigned *val) {
    unsigned char tmp;
    asm volatile("inb (%w1), %b0" : "=a"(tmp) : "d"(port));
    *val = tmp;
}

/*===========================================================================*
 *                              portw_out                                    *
 *===========================================================================*/
/* Output a word to an I/O port.
 * port: port number.
 * val: value to output.
 */
void portw_out(unsigned port, unsigned val) {
    asm volatile("outw %w0, (%w1)" : : "a"(val), "d"(port));
}

/*===========================================================================*
 *                              portw_in                                     *
 *===========================================================================*/
/* Input a word from an I/O port.
 * port: port number.
 * val: location to store the read word.
 */
void portw_in(unsigned port, unsigned *val) {
    unsigned short tmp;
    asm volatile("inw (%w1), %w0" : "=a"(tmp) : "d"(port));
    *val = tmp;
}

/*===========================================================================*
 *                              lock                                         *
 *===========================================================================*/
/* Disable interrupts and save flags. */
static u64_t lockvar;
void lock(void) { asm volatile("pushfq\n\tcli\n\tpop %0" : "=m"(lockvar)::"memory"); }

/*===========================================================================*
 *                              unlock                                       *
 *===========================================================================*/
/* Enable interrupts. */
void unlock(void) { asm volatile("sti"); }

/*===========================================================================*
 *                              restore                                      *
 *===========================================================================*/
/* Restore saved interrupt flags. */
void restore(void) { asm volatile("push %0\n\tpopfq" : : "m"(lockvar) : "memory"); }

/*===========================================================================*
 *                              build_sig                                    *
 *===========================================================================*/
/* Build a signal frame.
 * dst: buffer to construct the frame.
 * rp:  process receiving the signal.
 * sig: signal number.
 */
void build_sig(struct sig_info *dst, struct proc *rp, int sig) {
    dst->signo = sig;
    dst->sigpcpsw.rip = rp->p_pcpsw.rip;
    dst->sigpcpsw.rflags = rp->p_pcpsw.rflags;
}

/*===========================================================================*
 *                              get_chrome                                   *
 *===========================================================================*/
/* Return display type (stubbed). */
int get_chrome(void) { return 0; }

/*===========================================================================*
 *                              vid_copy                                     *
 *===========================================================================*/
/* Copy words to video memory.
 * buf:   source buffer or NULL for blanks.
 * base:  video memory base segment.
 * off:   offset within video memory.
 * words: number of words to copy.
 */
void vid_copy(void *buf, unsigned base, unsigned off, unsigned words) {
    if (!buf)
        return;
    u16_t *dst = (u16_t *)(uptr_t)(base + off);
    u16_t *src = buf;
    asm volatile("rep movsw"
                 : "=S"(src), "=D"(dst), "=c"(words)
                 : "0"(src), "1"(dst), "2"(words)
                 : "memory");
}

/*===========================================================================*
 *                              get_byte                                     *
 *===========================================================================*/
/* Fetch a byte from memory.
 * seg: segment value (unused).
 * off: offset within segment.
 */
unsigned char get_byte(unsigned seg, unsigned off) {
    unsigned char *p = (unsigned char *)(uptr_t)off;
    (void)seg;
    return *p;
}

/*===========================================================================*
 *                              reboot                                       *
 *===========================================================================*/
/* Halt the CPU. */
void reboot(void) { asm volatile("hlt"); }

/*===========================================================================*
 *                              wreboot                                      *
 *===========================================================================*/
/* Halt the CPU (warm reboot placeholder). */
void wreboot(void) { asm volatile("hlt"); }

/* Placeholders for assembly variables from original code. */
u64_t splimit;
