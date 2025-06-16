/* This task handles the interface between file system and kernel as well as
 * between memory manager and kernel.  System services are obtained by sending
 * sys_task() a message specifying what is needed.  To make life easier for
 * MM and FS, a library is provided with routines whose names are of the
 * form sys_xxx, e.g. sys_xit sends the SYS_XIT message to sys_task.  The
 * message types and parameters are:
 *
 *   SYS_FORK	 informs kernel that a process has forked
 *   SYS_NEWMAP	 allows MM to set up a process memory map
 *   SYS_EXEC	 sets program counter and stack pointer after EXEC
 *   SYS_XIT	 informs kernel that a process has exited
 *   SYS_GETSP	 caller wants to read out some process' stack pointer
 *   SYS_TIMES	 caller wants to get accounting times for a process
 *   SYS_ABORT	 MM or FS cannot go on; abort MINIX
 *   SYS_SIG	 send a signal to a process
 *   SYS_COPY	 requests a block of data to be copied between processes
 *
 * Message type m1 is used for all except SYS_SIG and SYS_COPY, both of
 * which need special parameter types.
 *
 *    m_type       PROC1     PROC2      PID     MEM_PTR
 * ------------------------------------------------------
 * | SYS_FORK   | parent  |  child  |   pid   |         |
 * |------------+---------+---------+---------+---------|
 * | SYS_NEWMAP | proc nr |         |         | map ptr |
 * |------------+---------+---------+---------+---------|
 * | SYS_EXEC   | proc nr |         | new sp  |         |
 * |------------+---------+---------+---------+---------|
 * | SYS_XIT    | parent  | exitee  |         |         |
 * |------------+---------+---------+---------+---------|
 * | SYS_GETSP  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------|
 * | SYS_TIMES  | proc nr |         | buf ptr |         |
 * |------------+---------+---------+---------+---------|
 * | SYS_ABORT  |         |         |         |         |
 * ------------------------------------------------------
 *
 *
 *    m_type       m6_i1     m6_i2     m6_i3     m6_f1
 * ------------------------------------------------------
 * | SYS_SIG    | proc_nr  |  sig    |         | handler |
 * ------------------------------------------------------
 *
 *
 *    m_type      m5_c1   m5_i1    m5_l1   m5_c2   m5_i2    m5_l2   m5_l3
 * --------------------------------------------------------------------------
 * | SYS_COPY   |src seg|src proc|src vir|dst seg|dst proc|dst vir| byte ct |
 * --------------------------------------------------------------------------
 *
 * In addition to the main sys_task() entry point, there are three other minor
 * entry points:
 *   cause_sig:	take action to cause a signal to occur, sooner or later
 *   inform:	tell MM about pending signals
 *   umap:	compute the physical address for a given virtual address
 */

#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/signal.h"
#include "../h/type.hpp" // Defines phys_bytes, vir_bytes etc.
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp" // Includes NIL_PROC definition
#include "type.hpp"
#include <cstddef> // For std::size_t, nullptr
#include <cstdint>
#include <cstdint> // For uint64_t, uintptr_t

#define COPY_UNIT 65534L /* max bytes to copy at once */

// extern phys_bytes umap(); // umap is defined later in this file, now returns uint64_t

PRIVATE message m;                      // Global message buffer, used by some functions here
PRIVATE char sig_stuff[SIG_PUSH_BYTES]; /* used to send signals to processes */

/*===========================================================================*
 *				sys_task				     *
 *===========================================================================*/
PUBLIC void sys_task() noexcept { // Added void return, noexcept
    /* Main entry point of sys_task.  Get the message and dispatch on type. */

    register int r;

    while (TRUE) {
        receive(ANY, &m);

        switch (m.m_type) { /* which system call */
        case SYS_FORK:
            r = do_fork(&m);
            break;
        case SYS_NEWMAP:
            r = do_newmap(&m);
            break;
        case SYS_EXEC:
            r = do_exec(&m);
            break;
        case SYS_XIT:
            r = do_xit(&m);
            break;
        case SYS_GETSP:
            r = do_getsp(&m);
            break;
        case SYS_TIMES:
            r = do_times(&m);
            break;
        case SYS_ABORT:
            r = do_abort(&m);
            break;
        case SYS_SIG:
            r = do_sig(&m);
            break;
        case SYS_COPY:
            r = do_copy(&m);
            break;
        default:
            r = ErrorCode::E_BAD_FCN;
        }

        m.m_type = r;         /* 'r' reports status of call */
        send(m.m_source, &m); /* send reply to caller */
    }
}

/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
// Modernized signature
static int do_fork(message *m_ptr) noexcept {
    /* Handle sys_fork().  'k1' has forked.  The child is 'k2'. */

    register struct proc *rpc;
    register char *sptr, *dptr; /* pointers for copying proc struct */
    int k1;                     /* number of parent process */
    int k2;                     /* number of child process */
    int pid;                    /* process id of child */
    std::uint64_t tok;          /* capability token */
    int bytes;                  /* counter for copying proc struct */

    k1 = proc1(*m_ptr);  /* extract parent slot number from msg */
    k2 = proc2(*m_ptr);  /* extract child slot number */
    pid = ::pid(*m_ptr); /* extract child process id */
    tok = token(*m_ptr); /* capability token */

    if (k1 < 0 || k1 >= NR_PROCS || k2 < 0 || k2 >= NR_PROCS)
        return (ErrorCode::E_BAD_PROC);
    rpc = proc_addr(k2);

    /* Copy parent 'proc' struct to child. */
    sptr = (char *)proc_addr(k1); /* parent pointer */
    dptr = (char *)proc_addr(k2); /* child pointer */
    bytes = sizeof(struct proc);  /* # bytes to copy */
    while (bytes--)
        *dptr++ = *sptr++; /* copy parent struct to child */

    rpc->p_flags |= NO_MAP;  /* inhibit the process from running */
    rpc->p_pid = pid;        /* install child's pid */
    rpc->p_reg[RET_REG] = 0; /* child sees pid = 0 to know it is child */
    rpc->p_token = tok;      /* install capability token */

    rpc->user_time = 0; /* set all the accounting times to 0 */
    rpc->sys_time = 0;
    rpc->child_utime = 0;
    rpc->child_stime = 0;
    return (OK);
}

/*===========================================================================*
 *				do_newmap				     *
 *===========================================================================*/
PRIVATE int do_newmap(message *m_ptr) /* pointer to request message */
{
    /* Handle sys_newmap().  Fetch the memory map from MM. */

    register struct proc *rp, *rsrc;
    uint64_t src_phys, dst_phys, pn; // phys_bytes -> uint64_t
    std::size_t vmm, vsys, vn;       // vir_bytes -> std::size_t
    int caller;                      /* whose space has the new map (usually MM) */
    int k;                           /* process whose map is to be loaded */
    int old_flags;                   /* value of flags before modification */
    struct mem_map *map_ptr;         /* virtual address of map inside caller (MM) */

    /* Extract message parameters and copy new memory map from MM. */
    caller = m_ptr->m_source;
    k = proc1(*m_ptr);
    map_ptr = reinterpret_cast<struct mem_map *>(mem_ptr(*m_ptr));
    if (k < -NR_TASKS || k >= NR_PROCS)
        return (ErrorCode::E_BAD_PROC);
    rp = proc_addr(k);                     /* ptr to entry of user getting new map */
    rsrc = proc_addr(caller);              /* ptr to MM's proc entry */
    vn = NR_SEGS * sizeof(struct mem_map); // sizeof returns size_t, vn is size_t
    pn = static_cast<uint64_t>(vn);        // pn is uint64_t
    vmm = reinterpret_cast<std::size_t>(map_ptr);
    vsys = reinterpret_cast<std::size_t>(rp->p_map); // rp->p_map is mem_map[]
    // umap now takes (..., std::size_t, std::size_t) and returns uint64_t
    if ((src_phys = umap(rsrc, D, vmm, vn)) == 0)
        panic("bad call to sys_newmap (src)", NO_NUM);
    if ((dst_phys = umap(proc_addr(SYSTASK), D, vsys, vn)) == 0)
        panic("bad call to sys_newmap (dst)", NO_NUM);
    // phys_copy now takes (uint64_t, uint64_t, uint64_t)
    phys_copy(src_phys, dst_phys, pn);

    old_flags = rp->p_flags; /* save the previous value of the flags */
    rp->p_flags &= ~NO_MAP;
    if (old_flags != 0 && rp->p_flags == 0)
        ready(rp);
    return (OK);
}

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
// Modernized signature
static int do_exec(message *m_ptr) noexcept {
    /* Handle sys_exec().  A process has done a successful EXEC. Patch it up. */

    register struct proc *rp;
    int k;            /* which process */
    uintptr_t sp_val; /* new sp value from message (was char*, treat as address value) */
    std::uint64_t tok;

    k = proc1(*m_ptr); /* 'k' tells which process did EXEC */
    sp_val = reinterpret_cast<uintptr_t>(stack_ptr(*m_ptr));
    tok = token(*m_ptr);
    if (k < 0 || k >= NR_PROCS)
        return (ErrorCode::E_BAD_PROC);
    rp = proc_addr(k);
    rp->p_sp = static_cast<uint64_t>(sp_val); /* set the stack pointer (p_sp is uint64_t) */
    rp->p_pcpsw.pc = nullptr;                 /* reset pc (function pointer to nullptr) */
    rp->p_alarm = 0;           /* reset alarm timer (p_alarm is real_time -> int64_t) */
    rp->p_flags &= ~RECEIVING; /* MM does not reply to EXEC call */
    if (rp->p_flags == 0)
        ready(rp);
    rp->p_token = tok; /* update capability token */
    set_name(k, sp);   /* save command string for F1 display */
    return (OK);
}

/*===========================================================================*
 *				do_xit					     *
 *===========================================================================*/
// Modernized signature
static int do_xit(message *m_ptr) noexcept {
    /* Handle sys_xit().  A process has exited. */

    register struct proc *rp, *rc;
    struct proc *np, *xp;
    int parent;  /* number of exiting proc's parent */
    int proc_nr; /* number of process doing the exit */

    parent = proc1(*m_ptr);  /* slot number of parent process */
    proc_nr = proc2(*m_ptr); /* slot number of exiting process */
    if (parent < 0 || parent >= NR_PROCS || proc_nr < 0 || proc_nr >= NR_PROCS)
        return (ErrorCode::E_BAD_PROC);
    rp = proc_addr(parent);
    rc = proc_addr(proc_nr);
    rp->child_utime += rc->user_time + rc->child_utime; /* accum child times */
    rp->child_stime += rc->sys_time + rc->child_stime;
    unready(rc);
    rc->p_alarm = 0;              /* turn off alarm timer */
    set_name(proc_nr, (char *)0); /* disable command printing for F1 */

    /* If the process being terminated happens to be queued trying to send a
     * message (i.e., the process was killed by a signal, rather than it doing an
     * EXIT), then it must be removed from the message queues.
     */
    if (rc->p_flags & SENDING) {
        /* Check all proc slots to see if the exiting process is queued. */
        for (rp = &proc[0]; rp < &proc[NR_TASKS + NR_PROCS]; rp++) {
            if (rp->p_callerq == nullptr) // NIL_PROC -> nullptr
                continue;
            if (rp->p_callerq == rc) {
                /* Exiting process is on front of this queue. */
                rp->p_callerq = rc->p_sendlink;
                break;
            } else {
                /* See if exiting process is in middle of queue. */
                np = rp->p_callerq;
                while ((xp = np->p_sendlink) != nullptr) // NIL_PROC -> nullptr
                    if (xp == rc) {
                        np->p_sendlink = xp->p_sendlink;
                        break;
                    } else {
                        np = xp;
                    }
            }
        }
    }
    rc->p_flags = P_SLOT_FREE;
    return (OK);
}

/*===========================================================================*
 *				do_getsp				     *
 *===========================================================================*/
// Modernized signature
static int do_getsp(message *m_ptr) noexcept {
    /* Handle sys_getsp().  MM wants to know what sp is. */

    register struct proc *rp;
    int k; /* whose stack pointer is wanted? */

    k = proc1(*m_ptr);
    if (k < 0 || k >= NR_PROCS)
        return (ErrorCode::E_BAD_PROC);
    rp = proc_addr(k);
    // m is the global message buffer.
    stack_ptr(m) = reinterpret_cast<char *>(static_cast<uintptr_t>(rp->p_sp));
    return (OK);
}

/*===========================================================================*
 *				do_times				     *
 *===========================================================================*/
// Modernized signature
static int do_times(message *m_ptr) noexcept {
    /* Handle sys_times().  Retrieve the accounting information. */

    register struct proc *rp;
    int k;

    k = proc1(*m_ptr); /* k tells whose times are wanted */
    if (k < 0 || k >= NR_PROCS)
        return (ErrorCode::E_BAD_PROC);
    rp = proc_addr(k);

    /* Insert the four times needed by the TIMES system call in the message. */
    // rp->user_time etc are real_time (int64_t). Message fields are long (modernized to int64_t in
    // h/type.hpp message struct).
    user_time(*m_ptr) = rp->user_time;
    system_time(*m_ptr) = rp->sys_time;
    child_utime(*m_ptr) = rp->child_utime;
    child_stime(*m_ptr) = rp->child_stime;
    return (OK);
}

/*===========================================================================*
 *				do_abort				     *
 *===========================================================================*/
// Modernized signature
static int do_abort(message *m_ptr) noexcept {
    /* Handle sys_abort.  MINIX is unable to continue.  Terminate operation. */
    (void)m_ptr;       // m_ptr is unused
    panic("", NO_NUM); // panic is noexcept
    return OK;         // Should not be reached if panic aborts, but to satisfy return type
}

/*===========================================================================*
 *				do_sig					     *
 *===========================================================================*/
// Modernized signature (already partially modernized for types)
static int do_sig(message *m_ptr) noexcept {
    /* Handle sys_sig(). Signal a process.  The stack is known to be big enough. */

    register struct proc *rp;
    uint64_t src_phys, dst_phys;            // phys_bytes -> uint64_t
    std::size_t vir_addr, sig_size, new_sp; // vir_bytes -> std::size_t
    int proc_nr;                            /* process number */
    int sig;                                /* signal number 1-16 */
    int (*sig_handler)();                   /* pointer to the signal handler */
    std::uint64_t tok;

    /* Extract parameters and prepare to build the words that get pushed. */
    proc_nr = pr(*m_ptr);       /* process being signalled */
    sig = signum(*m_ptr);       /* signal number, 1 to 16 */
    sig_handler = func(*m_ptr); /* run time system addr for catching sigs */
    tok = token(*m_ptr);
    if (proc_nr < LOW_USER || proc_nr >= NR_PROCS)
        return (ErrorCode::E_BAD_PROC);
    rp = proc_addr(proc_nr);
    if (tok != rp->p_token)
        return (ErrorCode::EACCES);
    vir_addr = reinterpret_cast<std::size_t>(sig_stuff); // sig_stuff is char[]
    new_sp = static_cast<std::size_t>(rp->p_sp); // rp->p_sp is uint64_t, new_sp is std::size_t

    /* Actually build the block of words to push onto the stack. */
    build_sig(sig_stuff, rp, sig); /* build up the info to be pushed */

    /* Prepare to do the push, and do it. */
    sig_size = SIG_PUSH_BYTES; // SIG_PUSH_BYTES is int const, sig_size is std::size_t
    new_sp -= sig_size;
    // umap takes (..., std::size_t, std::size_t) returns uint64_t
    src_phys = umap(proc_addr(SYSTASK), D, vir_addr, sig_size);
    dst_phys = umap(rp, S, new_sp, sig_size);
    if (dst_phys == 0)
        panic("do_sig can't signal; SP bad", NO_NUM); // panic is noexcept
    // phys_copy takes (uint64_t, uint64_t, uint64_t)
    phys_copy(src_phys, dst_phys, static_cast<uint64_t>(sig_size)); /* push pc, psw */

    /* Change process' sp and pc to reflect the interrupt. */
    rp->p_sp = static_cast<uint64_t>(new_sp); // new_sp is std::size_t, p_sp is uint64_t
    rp->p_pcpsw.pc =
        sig_handler; // sig_handler is int(*)(), p_pcpsw.pc is u64_t (func ptr)
                     // This assignment needs reinterpret_cast if types differ.
                     // Assuming p_pcpsw.pc being u64_t means it stores function address as integer.
    rp->p_pcpsw.pc =
        reinterpret_cast<decltype(rp->p_pcpsw.pc)>(reinterpret_cast<uintptr_t>(sig_handler));
    return (OK);
}

/*===========================================================================*
 *				do_copy					     *
 *===========================================================================*/
// Modernized signature (already partially modernized for types)
static int do_copy(message *m_ptr) noexcept {
    /* Handle sys_copy().  Copy data for MM or FS. */

    int src_proc, dst_proc, src_space, dst_space;
    std::size_t src_vir, dst_vir;       // vir_bytes -> std::size_t
    uint64_t src_phys, dst_phys, bytes; // phys_bytes -> uint64_t

    /* Dismember the command message. */
    src_proc = src_proc_nr(*m_ptr);
    dst_proc = dst_proc_nr(*m_ptr);
    src_space = src_space(*m_ptr);
    dst_space = dst_space(*m_ptr);
    src_vir = reinterpret_cast<std::size_t>(src_buffer(*m_ptr));
    dst_vir = reinterpret_cast<std::size_t>(dst_buffer(*m_ptr));
    bytes = static_cast<uint64_t>(copy_bytes(*m_ptr));

    /* Compute the source and destination addresses and do the copy. */
    if (src_proc == ABS)
        src_phys = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(src_buffer(*m_ptr)));
    else
        // umap takes (..., std::size_t, std::size_t) returns uint64_t
        // bytes (uint64_t) needs to be cast to std::size_t for umap's last param. This is a
        // potential narrowing.
        src_phys = umap(proc_addr(src_proc), src_space, src_vir, static_cast<std::size_t>(bytes));

    if (dst_proc == ABS)
        dst_phys = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dst_buffer(*m_ptr)));
    else
        dst_phys = umap(proc_addr(dst_proc), dst_space, dst_vir, static_cast<std::size_t>(bytes));

    if (src_phys == 0 || dst_phys == 0)
        return (ErrorCode::EFAULT);
    // phys_copy takes (uint64_t, uint64_t, uint64_t)
    phys_copy(src_phys, dst_phys, bytes);
    return (OK);
}

/*===========================================================================*
 *				cause_sig				     *
 *===========================================================================*/
// Modernized signature
PUBLIC void cause_sig(int proc_nr, int sig_nr) noexcept {
    /* A task wants to send a signal to a process.   Examples of such tasks are:
     *   TTY wanting to cause SIGINT upon getting a DEL
     *   CLOCK wanting to cause SIGALRM when timer expires
     * Signals are handled by sending a message to MM.  The tasks don't dare do
     * that directly, for fear of what would happen if MM were busy.  Instead they
     * call cause_sig, which sets bits in p_pending, and then carefully checks to
     * see if MM is free.  If so, a message is sent to it.  If not, when it becomes
     * free, a message is sent.  The calling task always gets control back from
     * cause_sig() immediately.
     */

    register struct proc *rp;

    rp = proc_addr(proc_nr);
    if (rp->p_pending == 0)
        sig_procs++; /* incr if a new proc is now pending */
    rp->p_pending |= 1 << (sig_nr - 1);
    inform(MM_PROC_NR); /* see if MM is free */
}

/*===========================================================================*
 *				inform					     *
 *===========================================================================*/
// Modernized signature
PUBLIC void inform(int proc_nr) noexcept {
    /* When a signal is detected by the kernel (e.g., DEL), or generated by a task
     * (e.g. clock task for SIGALRM), cause_sig() is called to set a bit in the
     * p_pending field of the process to signal.  Then inform() is called to see
     * if MM is idle and can be told about it.  Whenever MM blocks, a check is
     * made to see if 'sig_procs' is nonzero; if so, inform() is called.
     */

    register struct proc *rp, *mmp;

    /* If MM is not waiting for new input, forget it. */
    mmp = proc_addr(proc_nr);
    if (((mmp->p_flags & RECEIVING) == 0) || mmp->p_getfrom != ANY)
        return;

    /* MM is waiting for new input.  Find a process with pending signals. */
    for (rp = proc_addr(0); rp < proc_addr(NR_PROCS); rp++)
        if (rp->p_pending != 0) {
            m.m_type = KSIG; // m is global message buffer
            m.PROC1 = rp - proc - NR_TASKS;
            sig_map(m) = rp->p_pending;
            sig_procs--;
            if (mini_send(HARDWARE, proc_nr, &m) != OK)
                panic("can't inform MM", NO_NUM); // panic is noexcept
            rp->p_pending = 0;                    /* the ball is now in MM's court */
            return;
        }
}

/*===========================================================================*
 *				umap					     *
 *===========================================================================*/
// Signature already modernized in a previous step, adding noexcept
PUBLIC uint64_t umap(struct proc *rp, int seg, std::size_t vir_addr, std::size_t bytes) noexcept {
    // rp is struct proc*
    // seg is int
    // vir_addr is std::size_t (vir_bytes)
    // bytes is std::size_t (vir_bytes)
    // returns uint64_t (phys_bytes)
    /* Flat memory model: return virtual address as physical address. */
    if (bytes == 0) // bytes is unsigned std::size_t, so <= 0 becomes == 0
        return 0;   // Return uint64_t
    (void)rp;
    (void)seg;
    // Cast virtual address (std::size_t) to physical address (uint64_t)
    return static_cast<uint64_t>(vir_addr);
}
