#include "const.h"
#include "type.h"
#include "glo.h"
#include "proc.h"

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

/*---------------------------------------------------------------------------*/
PUBLIC void save(void)
{
    /* Real hardware register saving is not performed in this portable C
     * version.  The function only exists so that higher level code can
     * compile without change.
     */
}

/*---------------------------------------------------------------------------*/
PUBLIC void restart(void)
{
    /* Normally restores a process context and returns from interrupt.  In this
     * stub nothing is done.
     */
}

/*---------------------------------------------------------------------------*/
PUBLIC void s_call(int function, int src_dest, message *m_ptr)
{
    save();
    sys_call(function, cur_proc, src_dest, m_ptr);
    restart();
}

/*---------------------------------------------------------------------------*/
PUBLIC void tty_int(void)
{
    save();
    keyboard();
    restart();
}

/*---------------------------------------------------------------------------*/
PUBLIC void lpr_int(void)
{
    save();
    pr_char();
    restart();
}

/*---------------------------------------------------------------------------*/
PUBLIC void disk_int(void)
{
    message m;
    save();
    m.m_type = DISKINT;
    interrupt(FLOPPY, &m);
    restart();
}

/*---------------------------------------------------------------------------*/
PUBLIC void wini_int(void)
{
    message m;
    save();
    m.m_type = DISKINT;
    interrupt(WINI, &m);
    restart();
}

/*---------------------------------------------------------------------------*/
PUBLIC void clock_int(void)
{
    message m;
    save();
    m.m_type = CLOCK_TICK;
    interrupt(CLOCK, &m);
    restart();
}

/*---------------------------------------------------------------------------*/
PUBLIC void surprise(void)
{
    save();
    unexpected_int();
    restart();
}

/*---------------------------------------------------------------------------*/
PUBLIC void trp(void)
{
    save();
    trap();
    restart();
}

/*---------------------------------------------------------------------------*/
PUBLIC void divide(void)
{
    save();
    div_trap();
    restart();
}

/*---------------------------------------------------------------------------*/
PUBLIC void idle(void)
{
    /* Idle loop executed when no work is available. */
    for (;;) {
        asm volatile("hlt");
    }
}

