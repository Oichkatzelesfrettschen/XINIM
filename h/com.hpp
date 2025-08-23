#pragma once
/**
 * @file com.hpp
 * @brief Common constants for message passing and system call numbers.
 */

#include "const.hpp"
#include "type.hpp"

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
/**
 * @brief Accessor for the clock alarm interval.
 * @param m Reference to the message to access.
 * @return Reference to the tick interval field.
 */
inline int64_t &delta_ticks(message &m) noexcept { return m.m6_l1(); }
/**
 * @brief Accessor for the callback function pointer.
 * @param m Reference to the message to access.
 * @return Reference to the function pointer field.
 */
inline int (*&func_to_call(message &m) noexcept)() { return m.m6_f1(); }
/**
 * @brief Accessor for the new time value.
 * @param m Reference to the message to access.
 * @return Reference to the new time field.
 */
inline int64_t &new_time(message &m) noexcept { return m.m6_l1(); }
/**
 * @brief Accessor for the requesting process number.
 * @param m Reference to the message to access.
 * @return Reference to the process number field.
 */
inline int &clock_proc_nr(message &m) noexcept { return m.m6_i1(); }
/**
 * @brief Accessor for the remaining seconds field.
 * @param m Reference to the message to access.
 * @return Reference to the seconds left field.
 */
inline int64_t &seconds_left(message &m) noexcept { return m.m6_l1(); }

/* Accessors for block and character task messages. */
/**
 * @brief Accessor for the device identifier.
 * @param m Reference to the message to access.
 * @return Reference to the device field.
 */
inline int &device(message &m) noexcept { return m.m2_i1(); }
/**
 * @brief Accessor for the process number field.
 * @param m Reference to the message to access.
 * @return Reference to the process number field.
 */
inline int &proc_nr(message &m) noexcept { return m.m2_i2(); }
/**
 * @brief Accessor for the byte count field.
 * @param m Reference to the message to access.
 * @return Reference to the count field.
 */
inline int &count(message &m) noexcept { return m.m2_i3(); }
/**
 * @brief Accessor for the file position.
 * @param m Reference to the message to access.
 * @return Reference to the position field.
 */
inline int64_t &position(message &m) noexcept { return m.m2_l1(); }
/**
 * @brief Accessor for the user buffer address.
 * @param m Reference to the message to access.
 * @return Reference to the address field.
 */
inline char *&address(message &m) noexcept { return m.m2_p1(); }

/* Accessors for TTY task messages. */
/**
 * @brief Accessor for the terminal line number.
 * @param m Reference to the message to access.
 * @return Reference to the line number field.
 */
inline int &tty_line(message &m) noexcept { return m.m2_i1(); }
/**
 * @brief Accessor for the TTY request code.
 * @param m Reference to the message to access.
 * @return Reference to the request field.
 */
inline int &tty_request(message &m) noexcept { return m.m2_i3(); }
/**
 * @brief Accessor for the TTY speed field.
 * @param m Reference to the message to access.
 * @return Reference to the speed field.
 */
inline int64_t &tty_spek(message &m) noexcept { return m.m2_l1(); }
/**
 * @brief Accessor for the TTY flags field.
 * @param m Reference to the message to access.
 * @return Reference to the flags field.
 */
inline int64_t &tty_flags(message &m) noexcept { return m.m2_l2(); }

/* Accessors for reply messages from tasks. */
/**
 * @brief Accessor for the reply process number.
 * @param m Reference to the message to access.
 * @return Reference to the process number field.
 */
inline int &rep_proc_nr(message &m) noexcept { return m.m2_i1(); }
/**
 * @brief Accessor for the reply status field.
 * @param m Reference to the message to access.
 * @return Reference to the status field.
 */
inline int &rep_status(message &m) noexcept { return m.m2_i2(); }

/* Accessors for SYSTASK copy messages. */
/**
 * @brief Accessor for the source memory space specifier.
 * @param m Reference to the message to access.
 * @return Reference to the source space field.
 */
inline char &src_space(message &m) noexcept { return m.m5_c1(); }
/**
 * @brief Accessor for the source process number.
 * @param m Reference to the message to access.
 * @return Reference to the source process field.
 */
inline int &src_proc_nr(message &m) noexcept { return m.m5_i1(); }
/**
 * @brief Accessor for the source buffer address.
 * @param m Reference to the message to access.
 * @return Reference to the source buffer field.
 */
inline int64_t &src_buffer(message &m) noexcept { return m.m5_l1(); }
/**
 * @brief Accessor for the destination memory space specifier.
 * @param m Reference to the message to access.
 * @return Reference to the destination space field.
 */
inline char &dst_space(message &m) noexcept { return m.m5_c2(); }
/**
 * @brief Accessor for the destination process number.
 * @param m Reference to the message to access.
 * @return Reference to the destination process field.
 */
inline int &dst_proc_nr(message &m) noexcept { return m.m5_i2(); }
/**
 * @brief Accessor for the destination buffer address.
 * @param m Reference to the message to access.
 * @return Reference to the destination buffer field.
 */
inline int64_t &dst_buffer(message &m) noexcept { return m.m5_l2(); }
/**
 * @brief Accessor for the copy byte count.
 * @param m Reference to the message to access.
 * @return Reference to the byte count field.
 */
inline int64_t &copy_bytes(message &m) noexcept { return m.m5_l3(); }

/* Accessors for accounting and miscellaneous fields. */
/**
 * @brief Accessor for user time consumed.
 * @param m Reference to the message to access.
 * @return Reference to the user time field.
 */
inline int64_t &user_time(message &m) noexcept { return m.m4_l1(); }
/**
 * @brief Accessor for system time consumed.
 * @param m Reference to the message to access.
 * @return Reference to the system time field.
 */
inline int64_t &system_time(message &m) noexcept { return m.m4_l2(); }
/**
 * @brief Accessor for the children's user time.
 * @param m Reference to the message to access.
 * @return Reference to the child user time field.
 */
inline int64_t &child_utime(message &m) noexcept { return m.m4_l3(); }
/**
 * @brief Accessor for the children's system time.
 * @param m Reference to the message to access.
 * @return Reference to the child system time field.
 */
inline int64_t &child_stime(message &m) noexcept { return m.m4_l4(); }

/**
 * @brief Accessor for the first process identifier field.
 * @param m Reference to the message to access.
 * @return Reference to the first process field.
 */
inline int &proc1(message &m) noexcept { return m.m1_i1(); }
/**
 * @brief Accessor for the second process identifier field.
 * @param m Reference to the message to access.
 * @return Reference to the second process field.
 */
inline int &proc2(message &m) noexcept { return m.m1_i2(); }
/**
 * @brief Accessor for the process ID passed from MM.
 * @param m Reference to the message to access.
 * @return Reference to the PID field.
 */
inline int &pid(message &m) noexcept { return m.m1_i3(); }
/**
 * @brief Accessor for the stack pointer field.
 * @param m Reference to the message to access.
 * @return Reference to the stack pointer field.
 */
inline char *&stack_ptr(message &m) noexcept { return m.m1_p1(); }
/**
 * @brief Set capability token.
 * @param m    Message to update.
 * @param val  Token value.
 */
inline void set_token(message &m, std::uint64_t val) noexcept {
    m.m1_p2() = reinterpret_cast<char *>(val);
}
/**
 * @brief Retrieve capability token from a message.
 * @param m Message carrying the token.
 * @return Stored token value.
 */
inline std::uint64_t token(const message &m) noexcept {
    return reinterpret_cast<std::uint64_t>(m.m1_p2());
}
/**
 * @brief Accessor for the process number used by sys_sig.
 * @param m Reference to the message to access.
 * @return Reference to the process number field.
 */
inline int &pr(message &m) noexcept { return m.m6_i1(); }
/**
 * @brief Accessor for the signal number.
 * @param m Reference to the message to access.
 * @return Reference to the signal number field.
 */
inline int &signum(message &m) noexcept { return m.m6_i2(); }
/**
 * @brief Accessor for the signal handler function pointer.
 * @param m Reference to the message to access.
 * @return Reference to the function pointer field.
 */
inline int (*&func(message &m) noexcept)() { return m.m6_f1(); }
/**
 * @brief Accessor for the memory map pointer.
 * @param m Reference to the message to access.
 * @return Reference to the memory map pointer field.
 */
inline char *&mem_ptr(message &m) noexcept { return m.m1_p1(); }
/** Constant indicating a cancel request. */
inline constexpr int CANCEL = 0;
/**
 * @brief Accessor for the signal bit map.
 * @param m Reference to the message to access.
 * @return Reference to the signal map field.
 */
inline int &sig_map(message &m) noexcept { return m.m1_i2(); }
