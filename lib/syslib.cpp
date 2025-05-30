#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/const.h"
#include "../h/error.h"
#include "../h/type.h"
#include <signal.h>

#ifndef sighandler_t
typedef void (*sighandler_t)(int);
#endif

#define FS FS_PROC_NR
#define MM MMPROCNR

extern int errno;
extern message M;

/*----------------------------------------------------------------------------
            Messages to systask (special calls)
----------------------------------------------------------------------------*/

// Inform the kernel that a process has exited.
PUBLIC void sys_xit(int parent, int proc) {
    callm1(SYSTASK, SYS_XIT, parent, proc, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}

// Ask the kernel for the stack pointer of 'proc'.
PUBLIC void sys_getsp(int proc, vir_bytes *newsp) {
    callm1(SYSTASK, SYS_GETSP, proc, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    *newsp = (vir_bytes)M.STACK_PTR;
}

PUBLIC sys_sig(int proc, int sig, sighandler_t sighandler) {
    /* A proc has to be signaled.  Tell the kernel. */

    M.m6_i1 = proc;
    M.m6_i2 = sig;
    M.m6_f1 = sighandler;
    callx(SYSTASK, SYS_SIG);
}

// Notify the kernel that a fork occurred.
PUBLIC void sys_fork(int parent, int child, int pid) {
    callm1(SYSTASK, SYS_FORK, parent, child, pid, NIL_PTR, NIL_PTR, NIL_PTR);
}

// Tell the kernel process 'proc' executed a new image.
PUBLIC void sys_exec(int proc, char *ptr) {
    callm1(SYSTASK, SYS_EXEC, proc, 0, 0, ptr, NIL_PTR, NIL_PTR);
}

// Report that 'proc' received a new memory map.
PUBLIC void sys_newmap(int proc, char *ptr) {
    callm1(SYSTASK, SYS_NEWMAP, proc, 0, 0, ptr, NIL_PTR, NIL_PTR);
}

// Copy memory via the system task.
PUBLIC void sys_copy(message *mptr) {
    mptr->m_type = SYS_COPY;
    if (sendrec(SYSTASK, mptr) != OK)
        panic("sys_copy can't send", NO_NUM);
}

// Fetch process accounting times.
PUBLIC void sys_times(int proc, real_time ptr[4]) {
    callm1(SYSTASK, SYS_TIMES, proc, 0, 0, ptr, NIL_PTR, NIL_PTR);
    ptr[0] = M.USER_TIME;
    ptr[1] = M.SYSTEM_TIME;
    ptr[2] = M.CHILD_UTIME;
    ptr[3] = M.CHILD_STIME;
}

// Emergency system shutdown.
PUBLIC void sys_abort() { callm1(SYSTASK, SYS_ABORT, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }

// Inform the file system of an event.
PUBLIC void tell_fs(int what, int p1, int p2, int p3) {
    callm1(FS, what, p1, p2, p3, NIL_PTR, NIL_PTR, NIL_PTR);
}
