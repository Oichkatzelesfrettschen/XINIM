/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#include "../h/const.hpp"
#include "../h/type.hpp"
#include "const.hpp"

#undef EXTERN
#define EXTERN

#include "../h/callnr.hpp"
#include "glo.hpp"
#include "mproc.hpp" // Defines struct mproc, which uses modernized types
#include "param.hpp"
#include <cstdint>   // For uint16_t
#include <cstddef>   // For nullptr

/* Miscellaneous */
char core_name[] = {"core"}; /* file name where core images are produced */
uint16_t core_bits = 0x0EFC;  /* which signals cause core images (unshort -> uint16_t) */

extern char mm_stack[]; // From mm/const.hpp (MM_STACK_BYTES)
// MM_STACK_BYTES is std::size_t. Pointer arithmetic is fine.
char *stackpt = &mm_stack[MM_STACK_BYTES]; /* initial stack pointer */

// Modernized extern declarations, assuming they return int and are noexcept
// Actual return types (e.g. std::expected) would require call_vec to change type.
// For this task, we keep call_vec as int (*)() and assume these handlers conform or are wrapped.
extern int do_mm_exit() noexcept;
extern int do_fork() noexcept;
extern int do_wait() noexcept;
extern int do_brk() noexcept;
extern int do_getset() noexcept; // Covers GETPID, GETUID, GETGID, SETUID, SETGID
extern int do_exec() noexcept;
extern int do_signal() noexcept;
extern int do_kill() noexcept;
extern int do_pause() noexcept;
extern int do_alarm() noexcept;
extern int no_sys() noexcept;
// extern int unpause() noexcept; // unpause is not in the original list, might be from specific file
extern int do_ksig() noexcept;
extern int do_brk2() noexcept;


int (*call_vec[NCALLS])() = {
    no_sys,     /*  0 = unused	*/
    do_mm_exit, /*  1 = exit	*/
    do_fork,    /*  2 = fork	*/
    no_sys,     /*  3 = read	*/
    no_sys,     /*  4 = write	*/
    no_sys,     /*  5 = open	*/
    no_sys,     /*  6 = close	*/
    do_wait,    /*  7 = wait	*/
    no_sys,     /*  8 = creat	*/
    no_sys,     /*  9 = link	*/
    no_sys,     /* 10 = unlink	*/
    no_sys,     /* 11 = exec (actual exec is do_exec for exece)	*/
                // Note: MM handles EXEC, not a direct call here for old exec.
                // The do_exec is for exece (MM_EXEC) which is 59.
    no_sys,     /* 12 = chdir	*/
    no_sys,     /* 13 = time	*/
    no_sys,     /* 14 = mknod	*/
    no_sys,     /* 15 = chmod	*/
    no_sys,     /* 16 = chown	*/
    do_brk,     /* 17 = break	*/
    no_sys,     /* 18 = stat	*/
    no_sys,     /* 19 = lseek	*/
    do_getset,  /* 20 = getpid	*/
    no_sys,     /* 21 = mount	*/
    no_sys,     /* 22 = umount	*/
    do_getset,  /* 23 = setuid	*/
    do_getset,  /* 24 = getuid	*/
    no_sys,     /* 25 = stime	*/
    no_sys,     /* 26 = (ptrace)*/
    do_alarm,   /* 27 = alarm	*/
    no_sys,     /* 28 = fstat	*/
    do_pause,   /* 29 = pause	*/
    no_sys,     /* 30 = utime	*/
    no_sys,     /* 31 = (stty)	*/
    no_sys,     /* 32 = (gtty)	*/
    no_sys,     /* 33 = access	*/
    no_sys,     /* 34 = (nice)	*/
    no_sys,     /* 35 = (ftime)	*/
    no_sys,     /* 36 = sync	*/
    do_kill,    /* 37 = kill	*/
    no_sys,     /* 38 = unused	*/
    no_sys,     /* 39 = unused	*/
    no_sys,     /* 40 = unused	*/
    no_sys,     /* 41 = dup	*/
    no_sys,     /* 42 = pipe	*/
    no_sys,     /* 43 = times	*/
    no_sys,     /* 44 = (prof)	*/
    no_sys,     /* 45 = unused	*/
    do_getset,  /* 46 = setgid	*/
    do_getset,  /* 47 = getgid	*/
    do_signal,  /* 48 = sig	*/
    no_sys,     /* 49 = unused	*/
    no_sys,     /* 50 = unused	*/
    no_sys,     /* 51 = (acct)	*/
    no_sys,     /* 52 = (phys)	*/
    no_sys,     /* 53 = (lock)	*/
    no_sys,     /* 54 = ioctl	*/
    no_sys,     /* 55 = unused	*/
    no_sys,     /* 56 = (mpx)	*/
    no_sys,     /* 57 = unused	*/
    no_sys,     /* 58 = unused	*/
    do_exec,    /* 59 = exece	*/
    no_sys,     /* 60 = umask	*/
    no_sys,     /* 61 = chroot	*/
    no_sys,     /* 62 = unused	*/
    no_sys,     /* 63 = unused	*/

    do_ksig, /* 64 = KSIG: signals originating in the kernel	*/
    no_sys,  // Placeholder for unpause, assuming it's no_sys or needs specific declaration
    do_brk2, /* 66 = BRK2 (used to tell MM size of FS,INIT) */
    no_sys,  /* 67 = REVIVE	*/
    no_sys   /* 68 = TASK_REPLY	*/
};
