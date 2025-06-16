#pragma once
// Modernized for C++23

/* Here is the declaration of the process table.  Three assembly code routines
 * reference fields in it.  They are restart(), save(), and csv().  When
 * changing 'proc', be sure to change the field offsets built into the code.
 * It contains the process' registers, memory map, accounting, and message
 * send/receive information.
 */
#include "../h/const.hpp"      // Process table sizing constants
#include "../h/type.hpp"       // Message, mem_map, real_time, xinim types
#include "../include/defs.hpp" // Project-wide integer definitions
#include "./type.hpp"          // pc_psw definition
#include "const.hpp"           // Scheduling constants and printf macro
#ifdef printf
#undef printf
#endif

EXTERN struct proc {
    std::uint64_t p_reg[NR_REGS];  /* process' registers */
    xinim::virt_addr_t p_sp;       /* stack pointer - Formerly u64_t */
    struct pc_psw p_pcpsw;         /* pc and psw as pushed by interrupt */
    int p_flags;                   /* P_SLOT_FREE, SENDING, RECEIVING, etc. */
    struct mem_map p_map[NR_SEGS]; /* memory map */
    xinim::virt_addr_t p_splimit;  /* lowest legal stack value - Formerly u64_t */
    xinim::pid_t p_pid;            /* process id passed in from MM - Formerly int */
    std::uint64_t p_token;         /**< Capability token for privileged operations. */

    real_time user_time;   /* user time in ticks (real_time -> xinim::time_t) */
    real_time sys_time;    /* sys time in ticks (real_time -> xinim::time_t) */
    real_time child_utime; /* cumulative user time of children (real_time -> xinim::time_t) */
    real_time child_stime; /* cumulative sys time of children (real_time -> xinim::time_t) */
    real_time p_alarm;     /* time of next alarm in ticks, or 0 (real_time -> xinim::time_t) */

    struct proc *p_callerq;  /* head of list of procs wishing to send */
    struct proc *p_sendlink; /* link to next proc wishing to send */
    message *p_messbuf;      /* pointer to message buffer */
    int p_getfrom;           /* from whom does process want to receive? */

    struct proc *p_nextready; /* pointer to next ready process */
    int p_pending;            /* bit map for pending signals 1-16 */
    xinim::phys_addr_t cr3;   /* page table base - Formerly u64_t */
    int p_priority;           /* scheduling priority */
    int p_cpu;                /* CPU affinity */
} proc[NR_TASKS + NR_PROCS];

/* Bits for p_flags in proc[].  A process is runnable iff p_flags == 0 */
inline constexpr unsigned int P_SLOT_FREE = 001; /* set when slot is not in use */
inline constexpr unsigned int NO_MAP = 002;      /* keeps unmapped forked child from running */
inline constexpr unsigned int SENDING = 004;     /* set when process blocked trying to send */
inline constexpr unsigned int RECEIVING = 010;   /* set when process blocked trying to recv */

#define proc_addr(n) &proc[NR_TASKS + n] // Macro for pointer arithmetic, can be kept
inline constexpr struct proc *NIL_PROC = nullptr;

EXTERN struct proc *proc_ptr;                        /* &proc[cur_proc] */
EXTERN struct proc *bill_ptr;                        /* ptr to process to bill for clock ticks */
EXTERN struct proc *rdy_head[NR_CPUS][SCHED_QUEUES]; /* per-CPU ready list heads */
EXTERN struct proc *rdy_tail[NR_CPUS][SCHED_QUEUES]; /* per-CPU ready list tails */

EXTERN unsigned int busy_map;            /* bit map of busy tasks */
EXTERN message *task_mess[NR_TASKS + 1]; /* ptrs to messages for busy tasks */
