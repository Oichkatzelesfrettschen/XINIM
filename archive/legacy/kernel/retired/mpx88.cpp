#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"

/*
 * Simplified C replacement for the 8088 assembly scheduler/interrupt logic.
 * These stubs provide enough structure for building the kernel but do not
 * perform real context switching.
 */

extern void sys_call(int function, int caller, int src_dest, message *m_ptr); // Assuming sys_call is noexcept or handled
extern void keyboard() noexcept;
extern void pr_char() noexcept;
extern void interrupt(int task, message *m_ptr); // Assuming interrupt is noexcept or handled
extern void unexpected_int() noexcept;
extern void trap() noexcept;
extern void div_trap() noexcept;

/*===========================================================================*
 *                              save                                         *
 *===========================================================================*/
/* Stub saving of process context. */
PUBLIC void save() noexcept { // (void) -> (), added noexcept
    /* Real hardware register saving is not performed in this portable C
     * version.  The function only exists so that higher level code can
     * compile without change.
     */
}

/*===========================================================================*
 *                              restart                                      *
 *===========================================================================*/
/* Stub restart after interrupt. */
PUBLIC void restart() noexcept { // (void) -> (), added noexcept
    /* Normally restores a process context and returns from interrupt.  In this
     * stub nothing is done.
     */
}

/*===========================================================================*
 *                              s_call                                       *
 *===========================================================================*/
/* Entry point for system calls.
 * function: call number.
 * src_dest: calling process.
 * m_ptr: pointer to message.
 */
PUBLIC void s_call(int function, int src_dest, message *m_ptr) {
    save();
    sys_call(function, cur_proc, src_dest, m_ptr);
    restart();
}

/*===========================================================================*
 *                              tty_int                                      *
 *===========================================================================*/
/* Keyboard interrupt handler stub. */
PUBLIC void tty_int() noexcept { // (void) -> (), added noexcept
    save(); // noexcept
    keyboard(); // noexcept
    restart(); // noexcept
}

/*===========================================================================*
 *                              lpr_int                                      *
 *===========================================================================*/
/* Printer interrupt handler stub. */
PUBLIC void lpr_int() noexcept { // (void) -> (), added noexcept
    save(); // noexcept
    pr_char(); // noexcept
    restart(); // noexcept
}

/*===========================================================================*
 *                              disk_int                                     *
 *===========================================================================*/
/* Floppy disk interrupt handler stub. */
PUBLIC void disk_int() noexcept { // (void) -> (), added noexcept
    message m;
    save(); // noexcept
    m.m_type = DISKINT;
    interrupt(FLOPPY, &m); // Assuming noexcept
    restart(); // noexcept
}

/*===========================================================================*
 *                              wini_int                                     *
 *===========================================================================*/
/* Winchester disk interrupt handler stub. */
PUBLIC void wini_int() noexcept { // (void) -> (), added noexcept
    message m;
    save(); // noexcept
    m.m_type = DISKINT;
    interrupt(WINI, &m); // Assuming noexcept
    restart(); // noexcept
}

/*===========================================================================*
 *                              clock_int                                    *
 *===========================================================================*/
/* Clock tick interrupt handler stub. */
PUBLIC void clock_int() noexcept { // (void) -> (), added noexcept
    message m;
    save();
    m.m_type = CLOCK_TICK;
    interrupt(CLOCK, &m);
    restart();
}

/*===========================================================================*
 *                              surprise                                     *
 *===========================================================================*/
/* Unexpected interrupt handler stub. */
PUBLIC void surprise() noexcept { // (void) -> (), added noexcept
    save(); // noexcept
    unexpected_int(); // noexcept
    restart(); // noexcept
}

/*===========================================================================*
 *                              trp                                          *
 *===========================================================================*/
/* General trap handler stub. */
PUBLIC void trp() noexcept { // (void) -> (), added noexcept
    save(); // noexcept
    trap(); // noexcept
    restart(); // noexcept
}

/*===========================================================================*
 *                              divide                                       *
 *===========================================================================*/
/* Divide trap handler stub. */
PUBLIC void divide() noexcept { // (void) -> (), added noexcept
    save(); // noexcept
    div_trap(); // noexcept
    restart(); // noexcept
}

/*===========================================================================*
 *                              idle                                         *
 *===========================================================================*/
/* Simple halt loop when no work is available. */
PUBLIC void idle() noexcept { // (void) -> (), added noexcept
    /* Idle loop executed when no work is available. */
    for (;;) {
        asm volatile("hlt");
    }
}
