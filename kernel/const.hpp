#pragma once
// Modernized for C++17

/* General constants used by the kernel. */

/* 64-bit configuration constants */
/* Register order: rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15 */
#define NR_REGS 15
#define INIT_PSW 0x0200
#define INIT_SP (uint64_t *)0x0
#define ES_REG 0
#define DS_REG 0
#define CS_REG 0
#define SS_REG 0
#define VECTOR_BYTES 1024
#define MEM_BYTES 0x100000000ULL
#define DIVIDE_VECTOR 0
#define CLOCK_VECTOR 32
#define KEYBOARD_VECTOR 33
#define XT_WINI_VECTOR 34
#define FLOPPY_VECTOR 35
#define PRINTER_VECTOR 36
#define SYS_VECTOR 48
#define AT_WINI_VECTOR 119
#define INT_CTL 0x20
#define INT_CTLMASK 0x21
#define INT2_CTL 0xA0
#define INT2_MASK 0xA1
#define ENABLE 0x20

#define TASK_STACK_BYTES 256 /* how many bytes for each task stack */
#define K_STACK_BYTES 256    /* how many bytes for the kernel stack */

#define RET_REG 0 /* system call return codes go in this reg */
#define IDLE -999 /* 'cur_proc' = IDLE means nobody is running */
/* Scheduler configuration */
#define SCHED_ROUND_ROBIN 0 /* set to 1 for simple round-robin */
#define NR_CPUS 1           /* number of CPUs (SMP placeholder) */

#if SCHED_ROUND_ROBIN
#define NQ 3       /* # of scheduling queues */
#define TASK_Q 0   /* ready tasks are scheduled via queue 0 */
#define SERVER_Q 1 /* ready servers are scheduled via queue 1 */
#define USER_Q 2   /* ready users are scheduled via queue 2 */
#define SCHED_QUEUES NQ
#else
#define NR_SCHED_QUEUES 16 /* number of priority queues */
#define PRI_TASK 0         /* task priority */
#define PRI_SERVER 2       /* servers such as MM/FS */
#define PRI_USER 8         /* default user process priority */
#define SCHED_QUEUES NR_SCHED_QUEUES
#endif

#define printf printk /* the kernel really uses printk, not printf */
