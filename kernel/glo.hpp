#pragma once
// Modernized for C++17

/* Global variables used in the kernel. */

/* Clocks and timers */
extern real_time realtime; /* real time clock */
extern int lost_ticks;     /* incremented when clock int can't send mess*/

/* Processes, signals, and messages. */
extern int cur_proc;     /* current process */
extern int prev_proc;    /* previous process */
extern int sig_procs;    /* number of procs with p_pending != 0 */
extern message int_mess; /* interrupt routines build message here */

/* CPU type. */
extern int olivetti; /* TRUE for Olivetti-style keyboard */
extern int pc_at;    /*  PC-AT type diskette drives (360K/1.2M) ? */
/* Current CPU id (for future SMP support) */
extern int current_cpu;

/* The kernel and task stacks. */
extern struct t_stack {
    int stk[TASK_STACK_BYTES / sizeof(int)];
} t_stack[NR_TASKS - 1]; /* task stacks; task = -1 never really runs */

extern char k_stack[K_STACK_BYTES]; /* The kernel stack. */
