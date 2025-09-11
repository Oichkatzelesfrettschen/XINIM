#include "../include/defs.hpp" // For uptr_t
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"      // For phys_bytes original definition, message struct
#include <cstdint>       // For uint64_t
#include <cstddef>       // For size_t (used in cp_mess)

/*
 * C translation of the old 8088 assembly helper routines.  These functions
 * provide basic memory copy, port I/O and miscellaneous helpers needed by
 * the kernel.  Only minimal inline assembly is used where direct hardware
 * access is required.
 */

/* Storage used by lock()/restore(). */
PUBLIC unsigned lockvar = 0;
/* Current stack limit for csv().  Not really used in this hosted version. */
PUBLIC unsigned splimit = 0;
/* Temporary variable used by vid_copy(). */
PUBLIC unsigned tmp = 0;
/* Table for interrupt vectors restored by reboot(). */
PUBLIC unsigned _vec_table[142];

/*===========================================================================*
 *                              phys_copy                                    *
 *===========================================================================*/
// Changed signature to be memcpy-like: (dst, src, count) and use pointers + size_t
PUBLIC void phys_copy(void *dst, const void *src, std::size_t num_bytes) noexcept {
    /* Copy bytes from one memory location to another using ordinary C. */
    // This function now behaves like a simple memcpy.
    // If it was truly meant for physical addresses passed as integers, the previous uptr_t casting was more relevant.
    // Assuming the goal is a standard byte copy function for kernel space virtual addresses.
    unsigned char *d = static_cast<unsigned char *>(dst);
    const unsigned char *s = static_cast<const unsigned char *>(src);
    while (num_bytes-- > 0) {
        *d++ = *s++;
    }
}

/*===========================================================================*
 *                              cp_mess                                      *
 *===========================================================================*/
PUBLIC void cp_mess(int src_proc_nr, uint64_t src_phys_addr, message *src_msg_ptr, uint64_t dst_phys_addr,
                    message *dst_msg_ptr) noexcept { // phys_bytes -> uint64_t
    /* Segment parameters are obsolete; just copy the structure. */
    (void)src_phys_addr; // Parameter name changed for clarity
    (void)dst_phys_addr; // Parameter name changed for clarity

    dst_msg_ptr->m_source = src_proc_nr; // Parameter name changed for clarity
    {
        /* Copy the remaining bytes one by one to avoid structure padding
         * issues on different compilers.
         */
        // Parameter names changed for clarity
        unsigned char *s = reinterpret_cast<unsigned char *>(&src_msg_ptr->m_type);
        unsigned char *d = reinterpret_cast<unsigned char *>(&dst_msg_ptr->m_type);
        std::size_t bytes_to_copy = sizeof(message) - sizeof(int); // Use std::size_t explicitly
        while (bytes_to_copy-- > 0)
            *d++ = *s++;
    }
}

/*===========================================================================*
 *                              port I/O                                      *
 *===========================================================================*/
PUBLIC void port_out(unsigned port, unsigned value) noexcept {
    /* Output one byte to an I/O port. */
    asm volatile("outb %b0, %w1" : : "a"(value), "Nd"(port));
}

PUBLIC void port_in(unsigned port, unsigned *value) noexcept {
    unsigned char v;
    asm volatile("inb %w1, %0" : "=a"(v) : "Nd"(port));
    *value = v;
}

PUBLIC void portw_out(unsigned port, unsigned value) noexcept {
    asm volatile("outw %w0, %w1" : : "a"(value), "Nd"(port));
}

PUBLIC void portw_in(unsigned port, unsigned *value) noexcept {
    unsigned short v;
    asm volatile("inw %w1, %0" : "=a"(v) : "Nd"(port));
    *value = v;
}

/*===========================================================================*
 *                              lock/unlock/restore                           *
 *===========================================================================*/
PUBLIC void lock() noexcept { // Changed (void) to ()
    /* Disable interrupts and remember previous flags. */
    asm volatile("pushf\n\tcli\n\tpop %0" : "=r"(lockvar)::"memory");
}

PUBLIC void unlock() noexcept { // Changed (void) to ()
    /* Re-enable interrupts. */
    asm volatile("sti");
}

PUBLIC void restore() noexcept { // Changed (void) to ()
    /* Restore interrupt flag to value saved by lock(). */
    asm volatile("push %0\n\tpopf" ::"r"(lockvar) : "memory");
}

/*===========================================================================*
 *                              build_sig                                    *
 *===========================================================================*/
PUBLIC void build_sig(u16_t *dst, struct proc *rp, int sig) noexcept {
    /* Construct the four word stack frame for signal delivery.  Only the
     * program counter and flags are stored in this portable version.
     */
    dst[0] = (u16_t)sig;
    dst[1] = (u16_t)rp->p_pcpsw.rip;    /* low 16 bits of PC */
    dst[2] = 0;                         /* CS is not used */
    dst[3] = (u16_t)rp->p_pcpsw.rflags; /* flags */
}

/*===========================================================================*
 *                              csv & cret                                   *
 *===========================================================================*/
PUBLIC void csv(unsigned bytes) noexcept {
    /* Dummy csv implementation just checks the stack limit. */
    if (splimit && (unsigned)(&bytes) < splimit)
        panic("Kernel stack overrun", cur_proc); // panic is noexcept
}

PUBLIC void cret() noexcept { /* Nothing to do when returning from a C routine in this version. */ } // Changed (void) to ()

/*===========================================================================*
 *                              get_chrome                                   *
 *===========================================================================*/
PUBLIC int get_chrome() noexcept { // Changed (void) to ()
    /* Ask the BIOS for the equipment list and check bit 0x30. */
    unsigned char v;
    asm volatile("int $0x11; movb %%al, %0" : "=r"(v)::"ax");
    return (v & 0x30) == 0x30 ? 0 : 1;
}

/*===========================================================================*
 *                              vid_copy                                     *
 *===========================================================================*/
PUBLIC void vid_copy(u16_t *buf, unsigned base, unsigned off, unsigned words) noexcept {
    /* Copy a sequence of words to video RAM.  The base value selects the
     * segment of video memory, while off is the starting offset within that
     * segment.  When buf is NULL the area is filled with blanks.
     */
    u16_t *dst = (u16_t *)((uptr_t)base << 4) + off;
    if (buf == NIL_PTR) {
        while (words-- > 0)
            *dst++ = 0x0700; /* BLANK */
    } else {
        while (words-- > 0)
            *dst++ = *buf++;
    }
}

/*===========================================================================*
 *                              get_byte                                     *
 *===========================================================================*/
PUBLIC unsigned char get_byte(unsigned seg, unsigned off) noexcept {
    /* Return a byte from an arbitrary segment:offset pair. */
    u8_t *p = (u8_t *)(((u32_t)seg << 4) + off);
    return *p;
}

/*===========================================================================*
 *                              reboot & wreboot                             *
 *===========================================================================*/
PUBLIC void reboot() noexcept { // Changed (void) to ()
    /* BIOS reboot sequence (simplified). */
    asm volatile("cli; int $0x19" ::: "memory");
}

PUBLIC void wreboot() noexcept { asm volatile("cli; int $0x16; int $0x19" ::: "memory"); } // Changed (void) to ()
