/* This file contains essentially all of the process and message handling.
 * It has two main entry points from the outside:
 *
 *   sys_call:   called when a process or task does SEND, RECEIVE or SENDREC
 *   interrupt:	called by interrupt routines to send a message to task
 *
 * It also has five minor entry points:
 *
 *   ready:	put a process on one of the ready queues so it can be run
 *   unready:	remove a process from the ready queues
 *   sched:	a process has run too long; schedule another one
 *   mini_send:	send a message (used by interrupt signals, etc.)
 *   pick_proc:	pick a process to run (used by system initialization)
 */

#include "proc.hpp"
#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "type.hpp"
#include <cstddef> // For std::size_t, nullptr
#include <cstdint> // For uint64_t

/*===========================================================================*
 *				interrupt				     *
 *===========================================================================*/
// Modernized signature
/**
 * @brief Handle a hardware interrupt by delivering a message to the target task.
 *
 * @param task   Task number to notify.
 * @param m_ptr  Message payload for the task.
 * @pre Interrupts are disabled and @p task is a valid kernel task.
 * @post Target task receives the interrupt message or it is queued.
 */
PUBLIC void interrupt(int task, message *m_ptr) {
    /* An interrupt has occurred.  Schedule the task that handles it. */

    int i, n, old_map, this_bit;

    /* Try to send the interrupt message to the indicated task. */
    this_bit = 1 << (-task);
    if (mini_send(HARDWARE, task, m_ptr) != OK) {
        /* The message could not be sent to the task; it was not waiting. */
        old_map = busy_map; /* save original map of busy tasks */
        if (task == CLOCK) {
            lost_ticks++;
        } else {
            busy_map |= this_bit;     /* mark task as busy */
            task_mess[-task] = m_ptr; /* record message pointer */
        }
    } else {
        /* Hardware interrupt was successfully sent as a message. */
        busy_map &= ~this_bit; /* turn off the bit in case it was on */
        old_map = busy_map;
    }

    /* See if any tasks that were previously busy are now listening for msgs. */
    if (old_map != 0) {
        for (i = 2; i <= NR_TASKS; i++) {
            /* Check each task looking for one with a pending interrupt. */
            if ((old_map >> i) & 1) {
                /* Task 'i' has a pending interrupt. */
                n = mini_send(HARDWARE, -i, task_mess[i]);
                if (n == OK)
                    busy_map &= ~(1 << i);
            }
        }
    }

    /* If a task has just been readied and a user is running, run the task. */
#if SCHED_ROUND_ROBIN
    if (rdy_head[current_cpu][TASK_Q] != nullptr &&
        (cur_proc >= 0 || cur_proc == IDLE)) // NIL_PROC -> nullptr
#else
    if (rdy_head[current_cpu][PRI_TASK] != nullptr &&
        (cur_proc >= 0 || cur_proc == IDLE)) // NIL_PROC -> nullptr
#endif
        pick_proc();
    pick_proc(); // Called twice intentionally? Or a typo? Assuming intentional for now.
}

/*===========================================================================*
 *				sys_call				     *
 *===========================================================================*/
// Modernized signature
/**
 * @brief Dispatcher for process messaging system calls.
 *
 * Validates parameters and routes the request to the appropriate send or
 * receive handler.
 *
 * @param function Mask describing SEND/RECEIVE.
 * @param caller   Caller process number.
 * @param src_dest Source or destination depending on operation.
 * @param m_ptr    Pointer to the caller's message.
 */
PUBLIC void sys_call(int function, int caller, int src_dest, message *m_ptr) {
    /* The only system calls that exist in MINIX are sending and receiving
     * messages.  These are done by trapping to the kernel with an INT instruction.
     * The trap is caught and sys_call() is called to send or receive a message (or
     * both).
     */

    struct proc *rp;
    int n;

    /* Check for bad system call parameters. */
    rp = proc_addr(caller);
    if (src_dest < -NR_TASKS || (src_dest >= NR_PROCS && src_dest != ANY)) {
        rp->p_reg[RET_REG] = static_cast<uint64_t>(ErrorCode::E_BAD_SRC);
        return;
    }
    if (function != BOTH && caller >= LOW_USER) {
        rp->p_reg[RET_REG] = static_cast<uint64_t>(ErrorCode::E_NO_PERM); /* users only do BOTH */
        return;
    }

    /* The parameters are ok. Do the call. */
    if (function & SEND) {
        n = mini_send(caller, src_dest, m_ptr); /* func = SEND or BOTH */
        if (function == SEND || n != OK)
            rp->p_reg[RET_REG] = n;
        if (n != OK)
            return; /* SEND failed */
    }

    if (function & RECEIVE) {
        n = mini_rec(caller, src_dest, m_ptr); /* func = RECEIVE or BOTH */
        rp->p_reg[RET_REG] = n;
    }
}

/*===========================================================================*
 *				mini_send				     *
 *===========================================================================*/
// Modernized signature
/**
 * @brief Send a message from one process to another.
 *
 * If the destination is waiting it is unblocked, otherwise the caller is
 * queued.
 *
 * @param caller Sending process number.
 * @param dest   Destination process number.
 * @param m_ptr  Message to send.
 * @return OK or an error code.
 */
PUBLIC int mini_send(int caller, int dest, message *m_ptr) {
    /* Send a message from 'caller' to 'dest'.  If 'dest' is blocked waiting for
     * this message, copy the message to it and unblock 'dest'.  If 'dest' is not
     * waiting at all, or is waiting for another source, queue 'caller'.
     */

    register struct proc *caller_ptr, *dest_ptr, *next_ptr;
    std::size_t vb;       // vir_bytes -> std::size_t
    std::size_t vlo, vhi; // vir_clicks -> std::size_t
    std::size_t len;      // vir_clicks -> std::size_t

    /* User processes are only allowed to send to FS and MM.  Check for this. */
    if (caller >= LOW_USER && (dest != FS_PROC_NR && dest != MM_PROC_NR))
        return (ErrorCode::E_BAD_DEST);
    caller_ptr = proc_addr(caller); /* pointer to source's proc entry */
    dest_ptr = proc_addr(dest);     /* pointer to destination's proc entry */
    if (dest_ptr->p_flags & P_SLOT_FREE)
        return (ErrorCode::E_BAD_DEST); /* dead dest */

    /* Check for messages wrapping around top of memory or outside data seg. */
    len = caller_ptr->p_map[D].mem_len;        // mem_len is vir_clicks (std::size_t)
    vb = reinterpret_cast<std::size_t>(m_ptr); // m_ptr is message*
    vlo = vb >> CLICK_SHIFT;                   /* vir click for bottom of message */
    vhi = (vb + MESS_SIZE - 1) >>
          CLICK_SHIFT; /* vir click for top of message. MESS_SIZE is sizeof(message) */
    // p_map[D].mem_vir is vir_clicks (std::size_t)
    if (vhi < vlo || vhi - caller_ptr->p_map[D].mem_vir >= len)
        return (ErrorCode::E_BAD_ADDR);

    /* Check to see if 'dest' is blocked waiting for this message. */
    if ((dest_ptr->p_flags & RECEIVING) &&
        (dest_ptr->p_getfrom == ANY || dest_ptr->p_getfrom == caller)) {
        /* Destination is indeed waiting for this message. */
        cp_mess(caller, caller_ptr->p_map[D].mem_phys, m_ptr, dest_ptr->p_map[D].mem_phys,
                dest_ptr->p_messbuf);
        dest_ptr->p_flags &= ~RECEIVING; /* deblock destination */
        if (dest_ptr->p_flags == 0)
            ready(dest_ptr);
    } else {
        /* Destination is not waiting.  Block and queue caller. */
        if (caller == HARDWARE)
            return (ErrorCode::E_OVERRUN);
        caller_ptr->p_messbuf = m_ptr;
        caller_ptr->p_flags |= SENDING;
        unready(caller_ptr);

        /* Process is now blocked.  Put in on the destination's queue. */
        if ((next_ptr = dest_ptr->p_callerq) == nullptr) { // NIL_PROC -> nullptr
            dest_ptr->p_callerq = caller_ptr;
        } else {
            while (next_ptr->p_sendlink != nullptr) // NIL_PROC -> nullptr
                next_ptr = next_ptr->p_sendlink;
            next_ptr->p_sendlink = caller_ptr;
        }
        caller_ptr->p_sendlink = nullptr; // NIL_PROC -> nullptr
    }
    return (OK);
}

/*===========================================================================*
 *				mini_rec				     *
 *===========================================================================*/
// Modernized signature, PRIVATE -> static
/**
 * @brief Receive a message for a process.
 *
 * If a suitable message is already queued it is copied and the sender
 * unblocked, otherwise the caller is blocked.
 *
 * @param caller Process waiting for a message.
 * @param src    Acceptable source or ANY.
 * @param m_ptr  Buffer to place the message.
 * @return OK when successful.
 */
static int mini_rec(int caller, int src, message *m_ptr) {
    /* A process or task wants to get a message.  If one is already queued,
     * acquire it and deblock the sender.  If no message from the desired source
     * is available, block the caller.  No need to check parameters for validity.
     * Users calls are always sendrec(), and mini_send() has checked already.
     * Calls from the tasks, MM, and FS are trusted.
     */

    register struct proc *caller_ptr, *sender_ptr, *prev_ptr;
    int sender;

    caller_ptr = proc_addr(caller); /* pointer to caller's proc structure */

    /* Check to see if a message from desired source is already available. */
    sender_ptr = caller_ptr->p_callerq;
    while (sender_ptr != nullptr) { // NIL_PROC -> nullptr
        sender = sender_ptr - proc - NR_TASKS;
        if (src == ANY || src == sender) {
            /* An acceptable message has been found. */
            // p_map[D].mem_phys is phys_clicks (uint64_t). cp_mess expects uint64_t.
            cp_mess(sender, sender_ptr->p_map[D].mem_phys, sender_ptr->p_messbuf,
                    caller_ptr->p_map[D].mem_phys, m_ptr);
            sender_ptr->p_flags &= ~SENDING; /* deblock sender */
            if (sender_ptr->p_flags == 0)
                ready(sender_ptr);
            if (sender_ptr == caller_ptr->p_callerq)
                caller_ptr->p_callerq = sender_ptr->p_sendlink;
            else
                prev_ptr->p_sendlink = sender_ptr->p_sendlink;
            return (OK);
        }
        prev_ptr = sender_ptr;
        sender_ptr = sender_ptr->p_sendlink;
    }

    /* No suitable message is available.  Block the process trying to receive. */
    caller_ptr->p_getfrom = src;
    caller_ptr->p_messbuf = m_ptr;
    caller_ptr->p_flags |= RECEIVING;
    unready(caller_ptr);

    /* If MM has just blocked and there are kernel signals pending, now is the
     * time to tell MM about them, since it will be able to accept the message.
     */
    if (sig_procs > 0 && caller == MM_PROC_NR && src == ANY)
        inform(MM_PROC_NR);
    return (OK);
}

/*===========================================================================*
 *				pick_proc				     *
 *===========================================================================*/
/**
 * @brief Choose the next process to run.
 *
 * Updates global scheduling pointers and picks from the ready queues.
 */
PUBLIC pick_proc() {
    /* Decide who to run now. */

    register int q; /* which queue to use */
#if SCHED_ROUND_ROBIN
    if (rdy_head[current_cpu][TASK_Q] != nullptr) // NIL_PROC -> nullptr
        q = TASK_Q;
    else if (rdy_head[current_cpu][SERVER_Q] != nullptr) // NIL_PROC -> nullptr
        q = SERVER_Q;
    else
        q = USER_Q;
#else
    for (q = 0; q < SCHED_QUEUES; q++) {
        if (rdy_head[current_cpu][q] != nullptr) // NIL_PROC -> nullptr
            break;
    }
#endif

    /* Set 'cur_proc' and 'proc_ptr'. If system is idle, set 'cur_proc' to a
     * special value (IDLE), and set 'proc_ptr' to point to an unused proc table
     * slot, namely, that of task -1 (HARDWARE), so save() will have somewhere to
     * deposit the registers when a interrupt occurs on an idle machine.
     * Record previous process so that when clock tick happens, the clock task
     * can find out who was running just before it began to run.  (While the
     * clock task is running, 'cur_proc' = CLOCKTASK. In addition, set 'bill_ptr'
     * to always point to the process to be billed for CPU time.
     */
    prev_proc = cur_proc;
    if (rdy_head[current_cpu][q] != nullptr) { // NIL_PROC -> nullptr
        /* Someone is runnable. */
        cur_proc = rdy_head[current_cpu][q] - proc - NR_TASKS;
        proc_ptr = rdy_head[current_cpu][q];
        if (cur_proc >= LOW_USER)
            bill_ptr = proc_ptr;
    } else {
        /* No one is runnable. */
        cur_proc = IDLE;
        proc_ptr = proc_addr(HARDWARE);
        bill_ptr = proc_ptr;
    }
}

/*===========================================================================*
 *				ready					     *
 *===========================================================================*/
// Modernized signature
/**
 * @brief Enqueue a runnable process.
 *
 * Inserts the process at the end of the appropriate ready queue.
 *
 * @param rp Process to enqueue.
 */
PUBLIC void ready(struct proc *rp) {
    /* Add 'rp' to the end of one of the queues of runnable processes. Three
     * queues are maintained:
     *   TASK_Q   - (highest priority) for runnable tasks
     *   SERVER_Q - (middle priority) for MM and FS only
     *   USER_Q   - (lowest priority) for user processes
     */

    register int q; /* queue index */
    int r;
    int cpu = rp->p_cpu;

    lock(); /* disable interrupts */
#if SCHED_ROUND_ROBIN
    r = (rp - proc) - NR_TASKS; /* task or proc number */
    q = (r < 0 ? TASK_Q : r < LOW_USER ? SERVER_Q : USER_Q);
#else
    q = rp->p_priority;
    if (q < 0)
        q = 0;
    if (q >= SCHED_QUEUES)
        q = SCHED_QUEUES - 1;
#endif

    /* See if the relevant queue is empty. */
    if (rdy_head[cpu][q] == nullptr) // NIL_PROC -> nullptr
        r_rdy_head[cpu][q] = rp;     /* add to empty queue */
    else
        rdy_tail[cpu][q]->p_nextready = rp; /* add to tail of nonempty queue */
    rdy_tail[cpu][q] = rp;                  /* new entry has no successor */
    rp->p_nextready = nullptr;              // NIL_PROC -> nullptr
    restore();                              /* restore interrupts to previous state */
}

/*===========================================================================*
 *				unready					     *
 *===========================================================================*/
// Modernized signature
/**
 * @brief Remove a process from the ready queues.
 *
 * Used when a process blocks or is killed.
 *
 * @param rp Process to remove.
 */
PUBLIC void unready(struct proc *rp) {
    /* A process has blocked. */

    register struct proc *xp;
    int r, q;
    int cpu = rp->p_cpu;

    lock(); /* disable interrupts */
#if SCHED_ROUND_ROBIN
    r = rp - proc - NR_TASKS;
    q = (r < 0 ? TASK_Q : r < LOW_USER ? SERVER_Q : USER_Q);
#else
    q = rp->p_priority;
    if (q < 0)
        q = 0;
    if (q >= SCHED_QUEUES)
        q = SCHED_QUEUES - 1;
#endif
    if ((xp = rdy_head[cpu][q]) == nullptr) // NIL_PROC -> nullptr
        return;
    if (xp == rp) {
        /* Remove head of queue */
        rdy_head[cpu][q] = xp->p_nextready;
        pick_proc();
    } else {
        /* Search body of queue.  A process can be made unready even if it is
         * not running by being sent a signal that kills it.
         */
        while (xp->p_nextready != rp)
            if ((xp = xp->p_nextready) == nullptr) // NIL_PROC -> nullptr
                return;
        xp->p_nextready = xp->p_nextready->p_nextready;
        while (xp->p_nextready != nullptr) // NIL_PROC -> nullptr
            xp = xp->p_nextready;
        rdy_tail[cpu][q] = xp;
    }
    restore(); /* restore interrupts to previous state */
}

/*===========================================================================*
 *				sched					     *
 *===========================================================================*/
/**
 * @brief Reschedule a process after it has exhausted its time slice.
 *
 * Performs a round-robin rotation within the current priority queue.
 *
 * @pre Ready queue for the current priority is non-empty.
 * @post Next runnable process is placed at queue head.
 * @warning SMP-aware load balancing remains a TODO.
 */
PUBLIC void sched() { // Modernized signature
    /* The current process has run too long.  If another low priority (user)
     * process is runnable, put the current process on the end of the user queue,
     * possibly promoting another user to head of the queue.
     */

    lock(); /* disable interrupts */
#if SCHED_ROUND_ROBIN
    if (rdy_head[USER_Q] == nullptr) { // NIL_PROC -> nullptr
        restore();                     /* restore interrupts to previous state */
        return;
    }

    /* One or more user processes queued. */
    rdy_tail[USER_Q]->p_nextready = rdy_head[USER_Q];
    rdy_tail[USER_Q] = rdy_head[USER_Q];
    rdy_head[USER_Q] = rdy_head[USER_Q]->p_nextready;
    rdy_tail[USER_Q]->p_nextready = nullptr; // NIL_PROC -> nullptr
#else
    int q = proc_ptr->p_priority;
    int cpu = proc_ptr->p_cpu;
    if (rdy_head[cpu][q] == nullptr ||
        rdy_head[cpu][q]->p_nextready == nullptr) { // NIL_PROC -> nullptr
        restore();
        return;
    }
    rdy_tail[cpu][q]->p_nextready = rdy_head[cpu][q];
    rdy_tail[cpu][q] = rdy_head[cpu][q];
    rdy_head[cpu][q] = rdy_head[cpu][q]->p_nextready;
    rdy_tail[cpu][q]->p_nextready = nullptr; // NIL_PROC -> nullptr
#endif
    pick_proc();
    restore(); /* restore interrupts to previous state */
}
