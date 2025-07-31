/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"

/*
 * Simplified C replacement for the 8088 assembly scheduler/interrupt logic.
 * These stubs provide enough structure for building the kernel but do not
 * perform real context switching.
 */

extern void sys_call(int function, int caller, int src_dest, message *m_ptr);
extern void keyboard(void);
extern void pr_char(void);
extern void interrupt(int task, message *m_ptr);
extern void unexpected_int(void);
extern void trap(void);
extern void div_trap(void);

/*===========================================================================*
 *                              save                                         *
 *===========================================================================*/
/* Stub saving of process context. */
PUBLIC void save(void) {
    /* Real hardware register saving is not performed in this portable C
     * version.  The function only exists so that higher level code can
     * compile without change.
     */
}

/*===========================================================================*
 *                              restart                                      *
 *===========================================================================*/
/* Stub restart after interrupt. */
PUBLIC void restart(void) {
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
PUBLIC void tty_int(void) {
    save();
    keyboard();
    restart();
}

/*===========================================================================*
 *                              lpr_int                                      *
 *===========================================================================*/
/* Printer interrupt handler stub. */
PUBLIC void lpr_int(void) {
    save();
    pr_char();
    restart();
}

/*===========================================================================*
 *                              disk_int                                     *
 *===========================================================================*/
/* Floppy disk interrupt handler stub. */
PUBLIC void disk_int(void) {
    message m;
    save();
    m.m_type = DISKINT;
    interrupt(FLOPPY, &m);
    restart();
}

/*===========================================================================*
 *                              wini_int                                     *
 *===========================================================================*/
/* Winchester disk interrupt handler stub. */
PUBLIC void wini_int(void) {
    message m;
    save();
    m.m_type = DISKINT;
    interrupt(WINI, &m);
    restart();
}

/*===========================================================================*
 *                              clock_int                                    *
 *===========================================================================*/
/* Clock tick interrupt handler stub. */
PUBLIC void clock_int(void) {
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
PUBLIC void surprise(void) {
    save();
    unexpected_int();
    restart();
}

/*===========================================================================*
 *                              trp                                          *
 *===========================================================================*/
/* General trap handler stub. */
PUBLIC void trp(void) {
    save();
    trap();
    restart();
}

/*===========================================================================*
 *                              divide                                       *
 *===========================================================================*/
/* Divide trap handler stub. */
PUBLIC void divide(void) {
    save();
    div_trap();
    restart();
}

/*===========================================================================*
 *                              idle                                         *
 *===========================================================================*/
/* Simple halt loop when no work is available. */
PUBLIC void idle(void) {
    /* Idle loop executed when no work is available. */
    for (;;) {
        asm volatile("hlt");
    }
}
