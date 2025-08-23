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
#include "../h/signal.hpp"
#include "../h/type.hpp"
#include "../include/lib.hpp" // for send/receive primitives
#include "../include/shared/signal_constants.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include <cstddef> // For std::size_t, nullptr (though not directly used here for ptr)
#include <cstdint> // For int64_t

// External interfaces provided by other kernel modules
extern "C" void panic(const char *msg, int code) noexcept;
void sched();
void pr_char();
void port_out(unsigned port, unsigned value) noexcept;
void cause_sig(int proc_nr, int sig_nr) noexcept;

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
 * This function forms the main loop of the clock task. It waits for
 * messages indicating timer events or requests from other system components
 * and dispatches them to the appropriate handlers.
 *
 */
PUBLIC void clock_task() noexcept {
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
/**
 * @brief Program an alarm for a process or schedule a watchdog callback.
 *
 * When a user process requests an alarm the process receives a @c SIGALRM
 * signal once the specified number of clock ticks has elapsed. Kernel tasks
 * may request a callback function instead.
 *
 * @param m_ptr Message describing the alarm request.
*/
static void do_setalarm(message *m_ptr) noexcept {

    struct proc *rp{};
    int proc_nr{};             /* which process wants the alarm */
    int64_t delta_ticks_val{}; // interval in ticks
    int (*function)(){};       /* function to call (tasks only) */

    /* Extract the parameters from the message. */
    proc_nr = clock_proc_nr(*m_ptr);         /* process to interrupt later */
    delta_ticks_val = ::delta_ticks(*m_ptr); /* how many ticks to wait */
    function = func_to_call(*m_ptr);         /* function to call (tasks only) */
    rp = proc_addr(proc_nr);
    // mc is global message. SECONDS_LEFT (m2_l1) is int64_t. p_alarm, realtime are int64_t. HZ is
    // int.
    seconds_left(mc) = (rp->p_alarm == 0LL ? 0LL : (rp->p_alarm - realtime) / HZ);
    rp->p_alarm = (delta_ticks_val == 0LL ? 0LL : realtime + delta_ticks_val); // p_alarm is int64_t
    if (proc_nr < 0) // Assuming tasks are negative proc_nr
        watch_dog[-proc_nr] = function;

    /* Which alarm is next? */
    // MAX_P_LONG is int32_t. next_alarm is int64_t. Use INT64_MAX for true max.
    // For now, keep existing logic, but this might not represent the "latest possible" alarm.
    next_alarm = MAX_P_LONG;
    for (auto &entry : proc) {
        if (entry.p_alarm != 0LL && entry.p_alarm < next_alarm) {
            next_alarm = entry.p_alarm;
        }
    }
}

/*===========================================================================*
 *				do_get_time				     *
 *===========================================================================*/
/**
 * @brief Handle a GET_TIME request.
 *
 * Places the current time expressed in seconds since boot into the
 * global reply message.
 *
 */
static void do_get_time() noexcept {
    mc.m_type = REAL_TIME;
    new_time(mc) = boot_time + realtime / HZ; /* current real time */
}

/*===========================================================================*
 *				do_set_time				     *
 *===========================================================================*/
/**
 * @brief Set the system real time.
 *
 * Updates the base boot time so that subsequent calls to GET_TIME return
 * the supplied value. This call is normally restricted to privileged code.
 *
 * @param m_ptr Message containing the new time in seconds.
*/
static void do_set_time(message *m_ptr) noexcept { boot_time = new_time(*m_ptr) - realtime / HZ; }

/*===========================================================================*
 *				do_clocktick				     *
 *===========================================================================*/
/**
 * @brief Handle a single clock tick interrupt.
 *
 * Updates the system tick counters, processes expired alarms and
 * handles scheduling related bookkeeping. This function is invoked on
 * every timer interrupt.
 *
 * @pre Programmable interval timer was configured by ::init_clock.
 * @post Pending alarms are dispatched and scheduler counters updated.
 */
static void do_clocktick() noexcept {

    int t{}, proc_nr{};
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
        for (auto &entry : proc) {
            if (entry.p_alarm != 0LL) {
                /* See if this alarm time has been reached. */
                if (entry.p_alarm <= realtime) {
                    /* A timer has gone off.  If it is a user proc,
                     * send it a signal.  If it is a task, call the
                     * function previously specified by the task.
                     */
                    proc_nr = &entry - proc - NR_TASKS;
                    if (proc_nr >= 0)
                        cause_sig(proc_nr, xinim::signals::SIGALRM);
                    else
                        (*watch_dog[-proc_nr])();
                    entry.p_alarm = 0;
                }

                /* Work on determining which alarm is next. */
                if (entry.p_alarm != 0 && entry.p_alarm < next_alarm)
                    next_alarm = entry.p_alarm;
            }
        }
    }

    accounting(); /* keep track of who is using the cpu */

    /* If a user process has been running too long, pick another one. */
    if (--sched_ticks == 0) {
        if (bill_ptr == prev_ptr)
            /**
             * @brief Round-robin scheduling decision.
             * @warning Assumes a single CPU; SMP fairness needs review.
             */
            sched(); /* process has run too long */
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
 *
 * The global variable @c bill_ptr designates the process that should be
 * charged for the current tick. Depending on whether the system was in
 * user or kernel mode this function increments the corresponding counter.
 *
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
/**
 * @brief Initialise the programmable interval timer.
 *
 * Programs timer channel zero to generate periodic interrupts at the
 * system tick rate defined by @c HZ.
 *
 * @pre I/O ports @c TIMER0 and @c TIMER_MODE must be accessible.
 * @post Timer generates square-wave interrupts at @c HZ frequency.
 * @todo Replace legacy PIT with HPET/APIC timer for higher precision.
 */
static void init_clock() noexcept {

    unsigned int count, low_byte, high_byte;

    count = (unsigned)(IBM_FREQ / HZ); /* value to load into the timer */
    low_byte = count & BYTE;           /* compute low-order byte */
    high_byte = (count >> 8) & BYTE;   /* compute high-order byte */
    port_out(TIMER_MODE, SQUARE_WAVE); /* set timer to run continuously */
    port_out(TIMER0, low_byte);        /* load timer low byte */
    port_out(TIMER0, high_byte);       /* load timer high byte */
}
