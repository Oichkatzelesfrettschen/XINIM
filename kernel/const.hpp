#pragma once
// Modernized for C++17

/* General constants used by the kernel. */

#include "../../include/xinim/core_types.hpp" // For std::uint64_t, std::size_t, etc.
#include <cstdint> // For uint64_t
#include <cstddef> // For std::size_t

/* 64-bit configuration constants */
/* Register order: rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15 */
inline constexpr int NR_REGS = 15;
inline constexpr int INIT_PSW = 0x0200;
inline constexpr std::uint64_t* INIT_SP = nullptr; // Using std::uint64_t from core_types
inline constexpr int ES_REG = 0;
inline constexpr int DS_REG = 0;
inline constexpr int CS_REG = 0;
inline constexpr int SS_REG = 0;
inline constexpr std::size_t VECTOR_BYTES = 1024;
inline constexpr std::uint64_t MEM_BYTES = 0x100000000ULL; // Using std::uint64_t

inline constexpr int DIVIDE_VECTOR = 0;
inline constexpr int CLOCK_VECTOR = 32;
inline constexpr int KEYBOARD_VECTOR = 33;
inline constexpr int XT_WINI_VECTOR = 34;
inline constexpr int FLOPPY_VECTOR = 35;
inline constexpr int PRINTER_VECTOR = 36;
inline constexpr int SYS_VECTOR = 48;
inline constexpr int AT_WINI_VECTOR = 119;
inline constexpr int INT_CTL = 0x20;
inline constexpr int INT_CTLMASK = 0x21;
inline constexpr int INT2_CTL = 0xA0;
inline constexpr int INT2_MASK = 0xA1;
inline constexpr int ENABLE = 0x20;

inline constexpr std::size_t TASK_STACK_BYTES = 256; /* how many bytes for each task stack */
inline constexpr std::size_t K_STACK_BYTES = 256;    /* how many bytes for the kernel stack */

inline constexpr int RET_REG = 0; /* system call return codes go in this reg */
inline constexpr int IDLE = -999; /* 'cur_proc' = IDLE means nobody is running */

/* Scheduler configuration */
#define SCHED_ROUND_ROBIN 0 /* set to 1 for simple round-robin */
inline constexpr int NR_CPUS = 1;           /* number of CPUs (SMP placeholder) */

#if SCHED_ROUND_ROBIN
inline constexpr int NQ = 3;       /* # of scheduling queues */
inline constexpr int TASK_Q = 0;   /* ready tasks are scheduled via queue 0 */
inline constexpr int SERVER_Q = 1; /* ready servers are scheduled via queue 1 */
inline constexpr int USER_Q = 2;   /* ready users are scheduled via queue 2 */
inline constexpr int SCHED_QUEUES = NQ;
#else
inline constexpr int NR_SCHED_QUEUES = 16; /* number of priority queues */
inline constexpr int PRI_TASK = 0;         /* task priority */
inline constexpr int PRI_SERVER = 2;       /* servers such as MM/FS */
inline constexpr int PRI_USER = 8;         /* default user process priority */
inline constexpr int SCHED_QUEUES = NR_SCHED_QUEUES;
#endif

#define printf printk /* the kernel really uses printk, not printf */
