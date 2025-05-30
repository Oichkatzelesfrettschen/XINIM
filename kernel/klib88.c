#include "../include/defs.h"
#include "const.h"
#include "glo.h"
#include "proc.h"
#include "type.h"

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
PUBLIC void phys_copy(phys_bytes src, phys_bytes dst, phys_bytes count) {
    /* Copy bytes from one physical location to another using ordinary C. */
    unsigned char *s = (unsigned char *)(uptr_t)src;
    unsigned char *d = (unsigned char *)(uptr_t)dst;
    while (count-- > 0)
        *d++ = *s++;
}

/*===========================================================================*
 *                              cp_mess                                      *
 *===========================================================================*/
PUBLIC void cp_mess(int src, phys_bytes src_phys, message *src_ptr, phys_bytes dst_phys,
                    message *dst_ptr) {
    /* Segment parameters are obsolete; just copy the structure. */
    (void)src_phys;
    (void)dst_phys;

    dst_ptr->m_source = src;
    {
        /* Copy the remaining bytes one by one to avoid structure padding
         * issues on different compilers.
         */
        unsigned char *s = (unsigned char *)&src_ptr->m_type;
        unsigned char *d = (unsigned char *)&dst_ptr->m_type;
        size_t bytes = sizeof(message) - sizeof(int);
        while (bytes-- > 0)
            *d++ = *s++;
    }
}

/*===========================================================================*
 *                              port I/O                                      *
 *===========================================================================*/
PUBLIC void port_out(unsigned port, unsigned value) {
    /* Output one byte to an I/O port. */
    asm volatile("outb %b0, %w1" : : "a"(value), "Nd"(port));
}

PUBLIC void port_in(unsigned port, unsigned *value) {
    unsigned char v;
    asm volatile("inb %w1, %0" : "=a"(v) : "Nd"(port));
    *value = v;
}

PUBLIC void portw_out(unsigned port, unsigned value) {
    asm volatile("outw %w0, %w1" : : "a"(value), "Nd"(port));
}

PUBLIC void portw_in(unsigned port, unsigned *value) {
    unsigned short v;
    asm volatile("inw %w1, %0" : "=a"(v) : "Nd"(port));
    *value = v;
}

/*===========================================================================*
 *                              lock/unlock/restore                           *
 *===========================================================================*/
PUBLIC void lock(void) {
    /* Disable interrupts and remember previous flags. */
    asm volatile("pushf\n\tcli\n\tpop %0" : "=r"(lockvar)::"memory");
}

PUBLIC void unlock(void) {
    /* Re-enable interrupts. */
    asm volatile("sti");
}

PUBLIC void restore(void) {
    /* Restore interrupt flag to value saved by lock(). */
    asm volatile("push %0\n\tpopf" ::"r"(lockvar) : "memory");
}

/*===========================================================================*
 *                              build_sig                                    *
 *===========================================================================*/
PUBLIC void build_sig(u16_t *dst, struct proc *rp, int sig) {
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
PUBLIC void csv(unsigned bytes) {
    /* Dummy csv implementation just checks the stack limit. */
    if (splimit && (unsigned)(&bytes) < splimit)
        panic("Kernel stack overrun", cur_proc);
}

PUBLIC void cret(void) { /* Nothing to do when returning from a C routine in this version. */ }

/*===========================================================================*
 *                              get_chrome                                   *
 *===========================================================================*/
PUBLIC int get_chrome(void) {
    /* Ask the BIOS for the equipment list and check bit 0x30. */
    unsigned char v;
    asm volatile("int $0x11; movb %%al, %0" : "=r"(v)::"ax");
    return (v & 0x30) == 0x30 ? 0 : 1;
}

/*===========================================================================*
 *                              vid_copy                                     *
 *===========================================================================*/
PUBLIC void vid_copy(u16_t *buf, unsigned base, unsigned off, unsigned words) {
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
PUBLIC unsigned char get_byte(unsigned seg, unsigned off) {
    /* Return a byte from an arbitrary segment:offset pair. */
    u8_t *p = (u8_t *)(((u32_t)seg << 4) + off);
    return *p;
}

/*===========================================================================*
 *                              reboot & wreboot                             *
 *===========================================================================*/
PUBLIC void reboot(void) {
    /* BIOS reboot sequence (simplified). */
    asm volatile("cli; int $0x19" ::: "memory");
}

PUBLIC void wreboot(void) { asm volatile("cli; int $0x16; int $0x19" ::: "memory"); }
