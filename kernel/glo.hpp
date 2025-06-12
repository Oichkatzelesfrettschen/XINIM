#pragma once
// Modernized for C++17

/* Global variables used in the kernel. */

#include "../h/type.hpp"    // For real_time, message, xinim::pid_t (via core_types.hpp)
#include "./const.hpp"      // For TASK_STACK_BYTES, K_STACK_BYTES
// NR_TASKS is expected to be included via a general config header (e.g. h/const.hpp or defs.hpp)

/* Clocks and timers */
EXTERN real_time realtime;       /* real time clock (real_time -> xinim::time_t) */
EXTERN real_time lost_ticks;     /* incremented when clock int can't send mess, (real_time -> xinim::time_t) */


/* Processes, signals, and messages. */
EXTERN xinim::pid_t cur_proc;    /* current process - Formerly int */
EXTERN xinim::pid_t prev_proc;   /* previous process - Formerly int */
EXTERN int sig_procs;            /* number of procs with p_pending != 0 */
EXTERN message int_mess;         /* interrupt routines build message here */

/* CPU type. */
EXTERN bool olivetti;            /* TRUE for Olivetti-style keyboard - Formerly int */
EXTERN bool pc_at;               /* PC-AT type diskette drives (360K/1.2M) ? - Formerly int */
/* Current CPU id (for future SMP support) */
EXTERN int current_cpu;

/* The kernel and task stacks. */
EXTERN struct t_stack {
    // Using int for stack elements is typical; TASK_STACK_BYTES is std::size_t
    int stk[TASK_STACK_BYTES / sizeof(int)];
} t_stack[NR_TASKS - 1]; /* task stacks; task = -1 never really runs */

EXTERN char k_stack[K_STACK_BYTES]; /* The kernel stack. */
