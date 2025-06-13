#pragma once
// Modernized for C++17

#include "type.hpp" // Added include

/* System call function codes used with sendrec(). */
inline constexpr int SEND = 1;             /* function code for sending messages */
inline constexpr int RECEIVE = 2;          /* function code for receiving messages */
inline constexpr int BOTH = 3;             /* function code for SEND + RECEIVE */
inline constexpr int ANY = NR_PROCS + 100; /* receive(ANY, buf) accepts from any source */

/* Task numbers, function codes and reply codes. */
inline constexpr int HARDWARE = -1; /* used as source on interrupt generated msgs */

inline constexpr int SYSTASK = -2;   /* internal functions */
inline constexpr int SYS_XIT = 1;    /* fcn code for sys_xit(parent, proc) */
inline constexpr int SYS_GETSP = 2;  /* fcn code for sys_sp(proc, &new_sp) */
inline constexpr int SYS_SIG = 3;    /* fcn code for sys_sig(proc, sig) */
inline constexpr int SYS_FORK = 4;   /* fcn code for sys_fork(parent, child) */
inline constexpr int SYS_NEWMAP = 5; /* fcn code for sys_newmap(procno, map_ptr) */
inline constexpr int SYS_COPY = 6;   /* fcn code for sys_copy(ptr) */
inline constexpr int SYS_EXEC = 7;   /* fcn code for sys_exec(procno, new_sp) */
inline constexpr int SYS_TIMES = 8;  /* fcn code for sys_times(procno, bufptr) */
inline constexpr int SYS_ABORT = 9;  /* fcn code for sys_abort() */

inline constexpr int CLOCK = -3;     /* clock class */
inline constexpr int SET_ALARM = 1;  /* fcn code to CLOCK, set up alarm */
inline constexpr int CLOCK_TICK = 2; /* fcn code for clock tick */
inline constexpr int GET_TIME = 3;   /* fcn code to CLOCK, get real time */
inline constexpr int SET_TIME = 4;   /* fcn code to CLOCK, set real time */
inline constexpr int REAL_TIME = 1;  /* reply from CLOCK: here is real time */

inline constexpr int MEM = -4;     /* /dev/ram, /dev/(k)mem and /dev/null class */
inline constexpr int RAM_DEV = 0;  /* minor device for /dev/ram */
inline constexpr int MEM_DEV = 1;  /* minor device for /dev/mem */
inline constexpr int KMEM_DEV = 2; /* minor device for /dev/kmem */
inline constexpr int NULL_DEV = 3; /* minor device for /dev/null */

inline constexpr int FLOPPY = -5;     /* floppy disk class */
inline constexpr int WINCHESTER = -6; /* winchester (hard) disk class */
inline constexpr int DISKINT = 1;     /* fcn code for disk interrupt */
inline constexpr int DISK_READ = 3;   /* fcn code to DISK (must equal TTY_READ) */
inline constexpr int DISK_WRITE = 4;  /* fcn code to DISK (must equal TTY_WRITE) */
inline constexpr int DISK_IOCTL = 5;  /* fcn code for setting up RAM disk */

inline constexpr int TTY = -7;         /* terminal I/O class */
inline constexpr int PRINTER = -8;     /* printer I/O class */
inline constexpr int TTY_CHAR_INT = 1; /* fcn code for tty input interrupt */
inline constexpr int TTY_O_DONE = 2;   /* fcn code for tty output done */
inline constexpr int TTY_READ = 3;     /* fcn code for reading from tty */
inline constexpr int TTY_WRITE = 4;    /* fcn code for writing to tty */
inline constexpr int TTY_IOCTL = 5;    /* fcn code for ioctl */
inline constexpr int SUSPEND = -998;   /* used in interrupts when tty has no data */

/* Accessors for messages sent to the CLOCK task. */
// Message struct fields mX_lN are int64_t after h/type.hpp modernization
inline int64_t &delta_ticks(message &m) noexcept { return m.m6_l1(); }  /* alarm interval in clock ticks */
inline int (*&func_to_call(message &m) noexcept)() { return m.m6_f1(); } /* pointer to function to call */
inline int64_t &new_time(message &m) noexcept { return m.m6_l1(); }     /* value to set clock to (SET_TIME) */
inline int &clock_proc_nr(message &m) noexcept { // m6_i1 is int
    return m.m6_i1();
} /* which proc (or task) wants the alarm? */
inline int64_t &seconds_left(message &m) noexcept { return m.m6_l1(); } /* how many seconds were remaining */

/* Accessors for block and character task messages. */
inline int &device(message &m) noexcept { return m.m2_i1(); }    /* major-minor device */
inline int &proc_nr(message &m) noexcept { return m.m2_i2(); }   /* which (proc) wants I/O? */
inline int &count(message &m) noexcept { return m.m2_i3(); }     /* how many bytes to transfer */
inline int64_t &position(message &m) noexcept { return m.m2_l1(); } /* file offset */
inline char *&address(message &m) noexcept { return m.m2_p1(); } /* core buffer address */

/* Accessors for TTY task messages. */
inline int &tty_line(message &m) noexcept { return m.m2_i1(); }    /* terminal line */
inline int &tty_request(message &m) noexcept { return m.m2_i3(); } /* ioctl request code */
inline int64_t &tty_spek(message &m) noexcept { return m.m2_l1(); }   /* ioctl speed, erasing */
inline int64_t &tty_flags(message &m) noexcept { return m.m2_l2(); }  /* ioctl tty mode */

/* Accessors for reply messages from tasks. */
inline int &rep_proc_nr(message &m) noexcept { return m.m2_i1(); } /* proc for which I/O was done */
inline int &rep_status(message &m) noexcept { return m.m2_i2(); }  /* bytes transferred or error */

/* Accessors for SYSTASK copy messages. */
inline char &src_space(message &m) noexcept { return m.m5_c1(); }  /* T or D space */
inline int &src_proc_nr(message &m) noexcept { return m.m5_i1(); } /* process to copy from */
inline int64_t &src_buffer(message &m) noexcept { return m.m5_l1(); } /* virtual address source */
inline char &dst_space(message &m) noexcept { return m.m5_c2(); }  /* T or D space */
inline int &dst_proc_nr(message &m) noexcept { return m.m5_i2(); } /* process to copy to */
inline int64_t &dst_buffer(message &m) noexcept { return m.m5_l2(); } /* virtual address dest */
inline int64_t &copy_bytes(message &m) noexcept { return m.m5_l3(); } /* number of bytes to copy */

// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define DELTA_TICKS m6_l1()
// #define DELTA_TICKS m6_l1()   /* alarm interval in clock ticks */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define FUNC_TO_CALL m6_f1()
// #define FUNC_TO_CALL m6_f1()  /* pointer to function to call */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define NEW_TIME m6_l1()
// #define NEW_TIME m6_l1()      /* value to set clock to (SET_TIME) */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define CLOCK_PROC_NR m6_i1()
// #define CLOCK_PROC_NR m6_i1() /* which proc (or task) wants the alarm? */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define SECONDS_LEFT m6_l1()
// #define SECONDS_LEFT m6_l1()  /* how many seconds were remaining */

// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define DEVICE m2_i1()
// #define DEVICE m2_i1()   /* major-minor device */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define PROC_NR m2_i2()
// #define PROC_NR m2_i2()  /* which (proc) wants I/O? */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define COUNT m2_i3()
// #define COUNT m2_i3()    /* how many bytes to transfer */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define POSITION m2_l1()
// #define POSITION m2_l1() /* file offset */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define ADDRESS m2_p1()
// #define ADDRESS m2_p1()  /* core buffer address */

// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define TTY_LINE m2_i1()
// #define TTY_LINE m2_i1()    /* message parameter: terminal line */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define TTY_REQUEST m2_i3()
// #define TTY_REQUEST m2_i3() /* message parameter: ioctl request code */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define TTY_SPEK m2_l1()
// #define TTY_SPEK m2_l1()    /* message parameter: ioctl speed, erasing */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define TTY_FLAGS m2_l2()
// #define TTY_FLAGS m2_l2()   /* message parameter: ioctl tty mode */

// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define REP_PROC_NR m2_i1()
// #define REP_PROC_NR m2_i1() /* # of proc on whose behalf I/O was done */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define REP_STATUS m2_i2()
// #define REP_STATUS m2_i2()  /* bytes transferred or error number */

// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define SRC_SPACE m5_c1()
// #define SRC_SPACE m5_c1()   /* T or D space (stack is also D) */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define SRC_PROC_NR m5_i1()
// #define SRC_PROC_NR m5_i1() /* process to copy from */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define SRC_BUFFER m5_l1()
// #define SRC_BUFFER m5_l1()  /* virtual address where data come from */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define DST_SPACE m5_c2()
// #define DST_SPACE m5_c2()   /* T or D space (stack is also D) */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define DST_PROC_NR m5_i2()
// #define DST_PROC_NR m5_i2() /* process to copy to */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define DST_BUFFER m5_l2()
// #define DST_BUFFER m5_l2()  /* virtual address where data go to */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define COPY_BYTES m5_l3()
// #define COPY_BYTES m5_l3()  /* number of bytes to copy */

/* Accessors for accounting and miscellaneous fields. */
inline int64_t &user_time(message &m) noexcept { return m.m4_l1(); }   /* user time consumed */
inline int64_t &system_time(message &m) noexcept { return m.m4_l2(); } /* system time consumed */
inline int64_t &child_utime(message &m) noexcept { return m.m4_l3(); } /* user time of children */
inline int64_t &child_stime(message &m) noexcept { return m.m4_l4(); } /* system time of children */

inline int &proc1(message &m) noexcept { return m.m1_i1(); }       /* indicates a process */
inline int &proc2(message &m) noexcept { return m.m1_i2(); }       /* indicates a process */
inline int &pid(message &m) noexcept { return m.m1_i3(); }         /* process id passed from MM */
inline char *&stack_ptr(message &m) noexcept { return m.m1_p1(); } /* stack pointer */
inline int &pr(message &m) noexcept { return m.m6_i1(); }          /* process number for sys_sig */
inline int &signum(message &m) noexcept { return m.m6_i2(); }      /* signal number for sys_sig */
inline int (*&func(message &m) noexcept)() { return m.m6_f1(); }   /* function pointer for sys_sig */
inline char *&mem_ptr(message &m) noexcept { return m.m1_p1(); }   /* memory map pointer */
inline constexpr int CANCEL = 0;                                   /* request to cancel */
inline int &sig_map(message &m) noexcept { return m.m1_i2(); }     /* signal bit map */

// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define PROC1 m1_i1()
// #define PROC1 m1_i1()     /* indicates a process */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define PROC2 m1_i2()
// #define PROC2 m1_i2()     /* indicates a process */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define PID m1_i3()
// #define PID m1_i3()       /* process id passed from MM to kernel */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define STACK_PTR m1_p1()
// #define STACK_PTR m1_p1() /* used for stack ptr in sys_exec, sys_getsp */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define PR m6_i1()
// #define PR m6_i1()        /* process number for sys_sig */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define SIGNUM m6_i2()
// #define SIGNUM m6_i2()    /* signal number for sys_sig */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define FUNC m6_f1()
// #define FUNC m6_f1()      /* function pointer for sys_sig */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define MEM_PTR m1_p1()
// #define MEM_PTR m1_p1()   /* tells where memory map is for sys_newmap */
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define CANCEL 0
// #define CANCEL 0          /* general request to force a task to cancel */ // This is a value, not an accessor - should be constexpr int
// MODERNIZED: TODO: Remove macro, update callers to use direct message.accessor() methods. Example: was #define SIG_MAP m1_i2()
// #define SIG_MAP m1_i2()   /* used by kernel for passing signal bit map */
