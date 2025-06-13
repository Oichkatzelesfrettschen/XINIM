/* This file contains the code and data for the clock task.  The clock task
 * has a single entry point, clock_task().  It accepts four message types:
 *
 *   CLOCK_TICK:  a clock interrupt has occurred
 *   GET_TIME:    a process wants the real time
 *   SET_TIME:    a process wants to set the real time
 *   SET_ALARM:   a process wants to be alerted after a specified interval
 *
 * The input message is format m6.  The parameters are as follows:
 *
 *     m_type    CLOCK_PROC   FUNC    NEW_TIME
 * ---------------------------------------------
 * | SET_ALARM  | proc_nr  |f to call|  delta  |
 * |------------+----------+---------+---------|
 * | CLOCK_TICK |          |         |         |
 * |------------+----------+---------+---------|
 * | GET_TIME   |          |         |         |
 * |------------+----------+---------+---------|
 * | SET_TIME   |          |         | newtime |
 * ---------------------------------------------
 *
 * When an alarm goes off, if the caller is a user process, a SIGALRM signal
 * is sent to it.  If it is a task, a function specified by the caller will
 * be invoked.  This function may, for example, send a message, but only if
 * it is certain that the task will be blocked when the timer goes off.
 */

#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/signal.h"
#include "../h/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include <cstddef> // For std::size_t, nullptr (though not directly used here for ptr)
#include <cstdint> // For int64_t

/* Constant definitions. */
#define MILLISEC 100                      /* how often to call the scheduler (msec) */
#define SCHED_RATE (MILLISEC * HZ / 1000) /* number of ticks per schedule */

/* Clock parameters. */
#define TIMER0 0x40       /* port address for timer channel 0 */
#define TIMER_MODE 0x43   /* port address for timer channel 3 */
#define IBM_FREQ 1193182L /* IBM clock frequency for setting timer */
#define SQUARE_WAVE 0x36  /* mode for generating square wave */

/* Clock task variables. */
static int64_t boot_time;                 // real_time -> int64_t
static int64_t next_alarm;                // real_time -> int64_t
PRIVATE int sched_ticks = SCHED_RATE;     /* counter: when 0, call scheduler */
PRIVATE struct proc *prev_ptr;            /* last user process run by clock task */
PRIVATE message mc;                       /* message buffer for both input and output */
PRIVATE int (*watch_dog[NR_TASKS + 1])(); /* watch_dog functions to call */

/* Forward declarations for internal helpers. */
static void do_setalarm(message *m_ptr) noexcept;
static void do_get_time() noexcept;
static void do_set_time(message *m_ptr) noexcept;
static void do_clocktick() noexcept;
static void accounting() noexcept;
static void init_clock() noexcept;

/*===========================================================================*
 *				clock_task				     *
 *===========================================================================*/
/**
 * @brief Entry point for the clock task.
 *
 * Handles timer interrupts and time management requests from other tasks.
 */
PUBLIC void clock_task() noexcept { // Added void return, noexcept
    /* Main program of clock task.  It determines which of the 4 possible
     * calls this is by looking at 'mc.m_type'.   Then it dispatches.
     */

    int opcode;

    init_clock(); /* initialize clock tables */

    /* Main loop of the clock task.  Get work, process it, sometimes reply. */
    while (TRUE) {
        receive(ANY, &mc);  /* go get a message */
        opcode = mc.m_type; /* extract the function code */

        switch (opcode) {
        case SET_ALARM:
            do_setalarm(&mc);
            break;
        case GET_TIME:
            do_get_time();
            break;
        case SET_TIME:
            do_set_time(&mc);
            break;
        case CLOCK_TICK:
            do_clocktick();
            break;
        default:
            panic("clock task got bad message", mc.m_type);
        }

        /* Send reply, except for clock tick. */
        mc.m_type = OK;
        if (opcode != CLOCK_TICK)
            send(mc.m_source, &mc);
    }
}

/*===========================================================================*
 *				do_setalarm				     *
 *===========================================================================*/
static void do_setalarm(message *m_ptr) noexcept {
    /* A process wants an alarm signal or a task wants a given watch_dog function
     * called after a specified interval.  Record the request and check to see
     * it is the very next alarm needed.
     */

    register struct proc *rp;
    int proc_nr;         /* which process wants the alarm */
    int64_t delta_ticks; // Was long, for DELTA_TICKS (message field m2_l1 is int64_t)
    int (*function)();   /* function to call (tasks only) */

    /* Extract the parameters from the message. */
    proc_nr = clock_proc_nr(*m_ptr);   /* process to interrupt later */
    delta_ticks = delta_ticks(*m_ptr); /* how many ticks to wait */
    function = func_to_call(*m_ptr);   /* function to call (tasks only) */
    rp = proc_addr(proc_nr);
    // mc is global message. SECONDS_LEFT (m2_l1) is int64_t. p_alarm, realtime are int64_t. HZ is
    // int.
    seconds_left(mc) = (rp->p_alarm == 0LL ? 0LL : (rp->p_alarm - realtime) / HZ);
    rp->p_alarm = (delta_ticks == 0LL ? 0LL : realtime + delta_ticks); // p_alarm is int64_t
    if (proc_nr < 0) // Assuming tasks are negative proc_nr
        watch_dog[-proc_nr] = function;

    /* Which alarm is next? */
    // MAX_P_LONG is int32_t. next_alarm is int64_t. Use INT64_MAX for true max.
    // For now, keep existing logic, but this might not represent the "latest possible" alarm.
    next_alarm = MAX_P_LONG;
    for (rp = &proc[0]; rp < &proc[NR_TASKS + NR_PROCS]; rp++)
        if (rp->p_alarm != 0LL && rp->p_alarm < next_alarm) // p_alarm is int64_t
            next_alarm = rp->p_alarm;
}

/*===========================================================================*
 *				do_get_time				     *
 *===========================================================================*/
static void do_get_time() noexcept { // Changed (void) to ()
    /* Get and return the current clock time in ticks. */

    // mc is global message. NEW_TIME (m2_l1) is int64_t. boot_time, realtime are int64_t. HZ is
    // int.
    mc.m_type = REAL_TIME;
    new_time(mc) = boot_time + realtime / HZ; /* current real time */
}

/*===========================================================================*
 *				do_set_time				     *
 *===========================================================================*/
static void do_set_time(message *m_ptr) noexcept {
    /* Set the real time clock.  Only the superuser can use this call. */

    // boot_time, realtime are int64_t. m_ptr->NEW_TIME (m2_l1) is int64_t. HZ is int.
    boot_time = new_time(*m_ptr) - realtime / HZ;
}

/*===========================================================================*
 *				do_clocktick				     *
 *===========================================================================*/
static void do_clocktick() noexcept { // Changed (void) to ()
    /* This routine called on every clock tick. */

    register struct proc *rp;
    register int t, proc_nr;
    extern int pr_busy, pcount, cum_count, prev_ct;

    /* To guard against race conditions, first copy 'lost_ticks' to a local
     * variable, add this to 'realtime', and then subtract it from 'lost_ticks'.
     */
    t = lost_ticks;    /* 'lost_ticks' (int) counts missed interrupts */
    realtime += t + 1; /* update the time of day (realtime is int64_t) */
    lost_ticks -= t;   /* these interrupts are no longer missed */

    if (next_alarm <= realtime) { // Both int64_t
        /* An alarm may have gone off, but proc may have exited, so check. */
        next_alarm =
            MAX_P_LONG; /* start computing next alarm (MAX_P_LONG is int32_t, next_alarm int64_t) */
        for (rp = &proc[0]; rp < &proc[NR_TASKS + NR_PROCS]; rp++) {
            if (rp->p_alarm != 0LL) { // p_alarm is real_time (int64_t)
                /* See if this alarm time has been reached. */
                if (rp->p_alarm <= realtime) {
                    /* A timer has gone off.  If it is a user proc,
                     * send it a signal.  If it is a task, call the
                     * function previously specified by the task.
                     */
                    proc_nr = rp - proc - NR_TASKS;
                    if (proc_nr >= 0)
                        cause_sig(proc_nr, SIGALRM);
                    else
                        (*watch_dog[-proc_nr])();
                    rp->p_alarm = 0;
                }

                /* Work on determining which alarm is next. */
                if (rp->p_alarm != 0 && rp->p_alarm < next_alarm)
                    next_alarm = rp->p_alarm;
            }
        }
    }

    accounting(); /* keep track of who is using the cpu */

    /* If a user process has been running too long, pick another one. */
    if (--sched_ticks == 0) {
        if (bill_ptr == prev_ptr)
            sched();              /* process has run too long */
        sched_ticks = SCHED_RATE; /* reset quantum */
        prev_ptr = bill_ptr;      /* new previous process */

        /* Check if printer is hung up, and if so, restart it. */
        if (pr_busy && pcount > 0 && cum_count == prev_ct)
            pr_char();
        prev_ct = cum_count; /* record # characters printed so far */
    }
}

/*===========================================================================*
 *				accounting				     *
 *===========================================================================*/
/**
 * @brief Update user and system accounting statistics.
 */
static void accounting() noexcept { // Changed (void) to ()
    /* Update user and system accounting times.  The variable 'bill_ptr' is always
     * kept pointing to the process to charge for CPU usage.  If the CPU was in
     * user code prior to this clock tick, charge the tick as user time, otherwise
     * charge it as system time.
     */

    if (prev_proc >= LOW_USER)
        bill_ptr->user_time++; /* charge CPU time */
    else
        bill_ptr->sys_time++; /* charge system time */
}

/*===========================================================================*
 *				init_clock				     *
 *===========================================================================*/
static void init_clock() noexcept { // Changed (void) to ()
    /* Initialize channel 2 of the 8253A timer to e.g. 60 Hz. */

    unsigned int count, low_byte, high_byte;

    count = (unsigned)(IBM_FREQ / HZ); /* value to load into the timer */
    low_byte = count & BYTE;           /* compute low-order byte */
    high_byte = (count >> 8) & BYTE;   /* compute high-order byte */
    port_out(TIMER_MODE, SQUARE_WAVE); /* set timer to run continuously */
    port_out(TIMER0, low_byte);        /* load timer low byte */
    port_out(TIMER0, high_byte);       /* load timer high byte */
}
