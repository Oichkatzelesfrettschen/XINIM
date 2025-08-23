#include "../h/const.hpp"
#include "../h/type.hpp"
#include "../include/defs.h"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include <cstdint> // For uint64_t, uintptr_t, u16_t
#include <cstddef> // For size_t

/* Minimal 64-bit implementations of low level kernel routines. */

/*===========================================================================*
 *                              phys_copy                                    *
 *===========================================================================*/
/* Copy a block of memory. (memcpy style)
 * dst:  destination address.
 * src:  source address.
 * n: number of bytes to copy.
 */
// Standard memcpy-like signature: void *memcpy(void *dest, const void *src, size_t n);
void phys_copy(void *dst, const void *src, size_t n) noexcept {
    // Need to adjust asm input/output constraints if C variables change order or constness
    // Original: phys_copy(void *src, void *dst, long bytes)
    // asm volatile("rep movsb"
    //              : "=S"(src), "=D"(dst), "=c"(bytes) // Outputs: RSI, RDI, RCX
    //              : "0"(src), "1"(dst), "2"(bytes)  // Inputs: RSI=src, RDI=dst, RCX=bytes
    //              : "memory");
    // New: dst is arg0, src is arg1, n is arg2
    // RSI is traditionally source, RDI is destination.
    // Compiler will usually handle register allocation for "S", "D", "c".
    // Let's make input constraints explicit: "S"(src), "D"(dst), "c"(n)
    // And output constraints to reflect that these registers are clobbered.
    void *dst_out = dst;
    const void *src_out = src;
    size_t n_out = n;
    asm volatile("rep movsb"
                 : "+S"(src_out), "+D"(dst_out), "+c"(n_out) // Input/Output
                 : // No explicit inputs if using + operand
                 : "memory");
}


/*===========================================================================*
 *                              cp_mess                                      *
 *===========================================================================*/
/* Copy a message from one buffer to another.
 * src_proc_nr: source process number.
 * src_payload_ptr: pointer to message payload (after m_source) in source.
 * dst_payload_ptr: pointer to message payload (after m_source) in destination.
 */
// Assuming src_click, dst_click were unused placeholders.
// src_off, dst_off were effectively message_body_start_pointers.
void cp_mess(int src_proc_nr, const void *src_payload_ptr, void *dst_payload_ptr) noexcept {
    // (void)src_click; // Unused
    // (void)dst_click; // Unused
    // Original: int *d = dst_off; const int *s = src_off; d[0] = src;
    // This implies dst_off was message* and src_off was message*
    // The task is to copy the message content, m_source is set first.
    message* dst_msg = static_cast<message*>(dst_payload_ptr); // Assuming dst_payload_ptr is start of message struct
    const message* src_msg = static_cast<const message*>(src_payload_ptr); // Assuming src_payload_ptr is start of message struct

    dst_msg->m_source = src_proc_nr;

    // Copy rest of message (MESS_SIZE - sizeof(int) for m_source)
    // The assembly copies from offset 4 of src/dst.
    // This is highly dependent on the exact structure of 'message' and how these pointers are derived by caller.
    // For safety, a direct memcpy is better if the assembly's intent is a simple struct copy.
    // The assembly used relative offsets from these pointers.
    // Let's assume src_payload_ptr and dst_payload_ptr are pointers to the m_type field of message struct.
    // This matches the asm "lea 4(%[dst])" if dst points to m_source.
    // If they point to the start of the message struct:
    unsigned char *d_bytes = reinterpret_cast<unsigned char*>(dst_msg) + sizeof(int); // Skip m_source
    const unsigned char *s_bytes = reinterpret_cast<const unsigned char*>(src_msg) + sizeof(int); // Skip m_source
    size_t bytes_to_copy = sizeof(message) - sizeof(int);

    asm volatile("rep movsb"
                 : "+S"(s_bytes), "+D"(d_bytes), "+c"(bytes_to_copy)
                 :
                 : "memory");
}

/*===========================================================================*
 *                              port_out                                     *
 *===========================================================================*/
/* Output a byte to an I/O port.
 * port: port number.
 * val: value to output.
 */
void port_out(unsigned port, unsigned val) noexcept {
    asm volatile("outb %b0, (%w1)" : : "a"(val), "d"(port));
}

/*===========================================================================*
 *                              port_in                                      *
 *===========================================================================*/
/* Input a byte from an I/O port.
 * port: port number.
 * val: location to store the read byte.
 */
void port_in(unsigned port, unsigned *val) noexcept {
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
void portw_out(unsigned port, unsigned val) noexcept {
    asm volatile("outw %w0, (%w1)" : : "a"(val), "d"(port));
}

/*===========================================================================*
 *                              portw_in                                     *
 *===========================================================================*/
/* Input a word from an I/O port.
 * port: port number.
 * val: location to store the read word.
 */
void portw_in(unsigned port, unsigned *val) noexcept {
    unsigned short tmp;
    asm volatile("inw (%w1), %w0" : "=a"(tmp) : "d"(port));
    *val = tmp;
}

/*===========================================================================*
 *                              lock                                         *
 *===========================================================================*/
/* Disable interrupts and save flags. */
static u64_t lockvar;
void lock() noexcept { asm volatile("pushfq\n\tcli\n\tpop %0" : "=m"(lockvar)::"memory"); } // (void) -> ()

/*===========================================================================*
 *                              unlock                                       *
 *===========================================================================*/
/* Enable interrupts. */
void unlock() noexcept { asm volatile("sti"); } // (void) -> ()

/*===========================================================================*
 *                              restore                                      *
 *===========================================================================*/
/* Restore saved interrupt flags. */
void restore() noexcept { asm volatile("pushfq\n\tpopfq" : : "m"(lockvar) : "memory"); } // (void) -> ()

/*===========================================================================*
 *                              build_sig                                    *
 *===========================================================================*/
/* Build a signal frame.
 * dst: buffer to construct the frame.
 * rp:  process receiving the signal.
 * sig: signal number.
 */
void build_sig(struct sig_info *dst, struct proc *rp, int sig) noexcept {
    dst->signo = sig;
    dst->sigpcpsw.rip = rp->p_pcpsw.rip; // rip is u64_t in pc_psw (from kernel/type.hpp)
    dst->sigpcpsw.rflags = rp->p_pcpsw.rflags; // rflags is u64_t in pc_psw
}

/*===========================================================================*
 *                              get_chrome                                   *
 *===========================================================================*/
/* Return display type (stubbed). */
int get_chrome() noexcept { return 0; } // (void) -> ()

/*===========================================================================*
 *                              vid_copy                                     *
 *===========================================================================*/
/* Copy words to video memory.
 * buf:   source buffer or NULL for blanks.
 * base:  video memory base segment.
 * off:   offset within video memory.
 * words: number of words to copy.
 */
void vid_copy(void *buf, unsigned base_seg, unsigned off, size_t words) noexcept { // words to size_t
    // This logic assumes a segmented memory model for video RAM (base_seg << 4 + off)
    // or that base_seg is the flat base address and off is added to it.
    // The original klib88 used: u16_t *dst = (u16_t *)((uptr_t)base << 4) + off;
    // Here, base is 'unsigned'. If it's a segment, shift is needed. If flat, not.
    // Assuming 'base' is a segment value for this 64-bit context is unusual.
    // Let's assume 'base_seg' is the actual flat base address for video.
    uint16_t *dst = reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(base_seg) + off);

    if (!buf) { // Fill with blanks if buf is null
        // This part was missing in the original klib64.cpp, adding similar to klib88
        uint16_t blank_char = 0x0700; // Assuming blank space with light grey attribute
        size_t i; // Use size_t for loop counter with words
        for (i = 0; i < words; ++i) {
            dst[i] = blank_char;
        }
        return;
    }

    const uint16_t *src = static_cast<const uint16_t*>(buf);
    // The asm constraint for src ("S") and dst ("D") will use RSI and RDI.
    // Ensure words is in RCX via "c".
    asm volatile("rep movsw"
                 : "+S"(src), "+D"(dst), "+c"(words)
                 :
                 : "memory");
}

/*===========================================================================*
 *                              get_byte                                     *
 *===========================================================================*/
/* Fetch a byte from memory.
 * seg: segment value (unused in flat 64-bit model).
 * off: flat memory offset (address).
 */
unsigned char get_byte(unsigned seg, uintptr_t off) noexcept { // off to uintptr_t
    unsigned char *p = reinterpret_cast<unsigned char *>(off);
    (void)seg; // Mark unused
    return *p;
}

/*===========================================================================*
 *                              reboot                                       *
 *===========================================================================*/
/* Halt the CPU. */
void reboot() noexcept { asm volatile("hlt"); } // (void) -> ()

/*===========================================================================*
 *                              wreboot                                      *
 *===========================================================================*/
/* Halt the CPU (warm reboot placeholder). */
void wreboot() noexcept { asm volatile("hlt"); } // (void) -> ()
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
