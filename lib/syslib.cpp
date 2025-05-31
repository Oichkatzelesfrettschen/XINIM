#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../include/lib.hpp" // for message structure and constants
#include <signal.h>

#ifndef sighandler_t
typedef void (*sighandler_t)(int);
#endif

extern int errno;
extern message M;

/*----------------------------------------------------------------------------
            Messages to systask (special calls)
----------------------------------------------------------------------------*/

// Notify the kernel that process 'proc' has exited.
void sys_xit(int parent, int proc) {
    /* A proc has exited.  Tell the kernel. */

    callm1(SYSTASK, SYS_XIT, parent, proc, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}

// Ask the kernel for the stack pointer of process 'proc'.
void sys_getsp(int proc, vir_bytes *newsp) {
    /* Ask the kernel what the sp is. */

    callm1(SYSTASK, SYS_GETSP, proc, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    *newsp = (vir_bytes)M.STACK_PTR;
}

// Signal process 'proc' with signal 'sig'.
void sys_sig(int proc, int sig, sighandler_t sighandler) {
    /* A proc has to be signaled.  Tell the kernel. */

    M.m6_i1() = proc;
    M.m6_i2() = sig;
    M.m6_f1() = sighandler;
    callx(SYSTASK, SYS_SIG);
}

// Tell the kernel a process has forked.
void sys_fork(int parent, int child, int pid) {
    /* A proc has forked.  Tell the kernel. */

    callm1(SYSTASK, SYS_FORK, parent, child, pid, NIL_PTR, NIL_PTR, NIL_PTR);
}

// Tell the kernel a process has exec'd.
void sys_exec(int proc, char *ptr) {
    /* A proc has exec'd.  Tell the kernel. */

    callm1(SYSTASK, SYS_EXEC, proc, 0, 0, ptr, NIL_PTR, NIL_PTR);
}

// Notify the kernel of a new memory map for 'proc'.
void sys_newmap(int proc, char *ptr) {
    /* A proc has been assigned a new memory map.  Tell the kernel. */

    callm1(SYSTASK, SYS_NEWMAP, proc, 0, 0, ptr, NIL_PTR, NIL_PTR);
}

// Perform a copy on behalf of a user process.
void sys_copy(message *mptr) {
    /* A proc wants to use local copy. */

    /* Make this routine better.  Also check other guys' error handling -DEBUG */
    mptr->m_type = SYS_COPY;
    if (sendrec(SYSTASK, mptr) != OK)
        panic("sys_copy can't send", NO_NUM);
}

// Retrieve accounting times for process 'proc'.
void sys_times(int proc, real_time ptr[4]) {
    /* Fetch the accounting info for a proc. */

    callm1(SYSTASK, SYS_TIMES, proc, 0, 0, ptr, NIL_PTR, NIL_PTR);
    ptr[0] = M.USER_TIME;
    ptr[1] = M.SYSTEM_TIME;
    ptr[2] = M.CHILD_UTIME;
    ptr[3] = M.CHILD_STIME;
}

// Abort execution after an irrecoverable error.
void sys_abort() {
    /* Something awful has happened.  Abandon ship. */

    callm1(SYSTASK, SYS_ABORT, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}

// Inform the file system of a significant event.
int tell_fs(int what, int p1, int p2, int p3) {
    /* This routine is only used by MM to inform FS of certain events:
     *      tell_fs(CHDIR, slot, dir, 0)
     *      tell_fs(EXIT, proc, 0, 0)
     *      tell_fs(FORK, parent, child, 0)
     *      tell_fs(SETGID, proc, realgid, effgid)
     *      tell_fs(SETUID, proc, realuid, effuid)
     *      tell_fs(SYNC, 0, 0, 0)
     *      tell_fs(UNPAUSE, proc, signr, 0)
     */
    callm1(FS, what, p1, p2, p3, NIL_PTR, NIL_PTR, NIL_PTR);
}
