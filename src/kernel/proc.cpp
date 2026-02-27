/* This file contains essentially all of the process and message handling.
 */

#include "proc.hpp"
#include "sys/callnr.hpp"
#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "type.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {
    struct proc proc[NR_TASKS + NR_PROCS];
    struct proc *proc_ptr;                        
    struct proc *bill_ptr;                        
    struct proc *rdy_head[NR_CPUS][SCHED_QUEUES]; 
    struct proc *rdy_tail[NR_CPUS][SCHED_QUEUES]; 
    unsigned int busy_map;            
    message *task_mess[NR_TASKS + 1]; 
}

/*===========================================================================*
 *				interrupt				     *
 *===========================================================================*/
extern "C" void interrupt(int task, message *m_ptr) noexcept {
    int i, n;
    unsigned int old_map = 0U;
    unsigned int this_bit = 0U;

    this_bit = 1U << static_cast<unsigned int>(-task);
    if (mini_send(HARDWARE, task, m_ptr) != OK) {
        old_map = busy_map;
        if (task == CLOCK) {
            lost_ticks++;
        } else {
            busy_map |= this_bit;
            task_mess[-task] = m_ptr;
        }
    } else {
        busy_map &= ~this_bit;
        old_map = busy_map;
    }

    if (old_map != 0) {
        for (i = 2; i <= NR_TASKS; i++) {
            if ((old_map >> i) & 1) {
                n = mini_send(HARDWARE, -i, task_mess[i]);
                if (n == OK)
                    busy_map &= ~(1U << static_cast<unsigned int>(i));
            }
        }
    }

#if SCHED_ROUND_ROBIN
    if (rdy_head[current_cpu][TASK_Q] != nullptr &&
        (cur_proc >= 0 || cur_proc == IDLE))
#else
    if (rdy_head[current_cpu][PRI_TASK] != nullptr &&
        (cur_proc >= 0 || cur_proc == IDLE))
#endif
        pick_proc();
}

/*===========================================================================*
 *				sys_call				     *
 *===========================================================================*/
extern "C" void sys_call(int function, int caller, int src_dest, message *m_ptr) noexcept {
    struct proc *rp;
    int n;

    rp = proc_addr(caller);
    if (src_dest < -NR_TASKS || (src_dest >= NR_PROCS && src_dest != ANY)) {
        rp->p_reg[RET_REG] = static_cast<uint64_t>(ErrorCode::E_BAD_SRC);
        return;
    }
    if (function != BOTH && caller >= LOW_USER) {
        rp->p_reg[RET_REG] = static_cast<uint64_t>(ErrorCode::E_NO_PERM);
        return;
    }

    if (function & SEND) {
        n = mini_send(caller, src_dest, m_ptr);
        if (function == SEND || n != OK)
            rp->p_reg[RET_REG] = static_cast<std::uint64_t>(n);
        if (n != OK)
            return;
    }

    if (function & RECEIVE) {
        n = mini_rec(caller, src_dest, m_ptr);
        rp->p_reg[RET_REG] = static_cast<std::uint64_t>(n);
    }
}

/*===========================================================================*
 *				mini_send				     *
 *===========================================================================*/
extern "C" int mini_send(int caller, int dest, message *m_ptr) noexcept {
    struct proc *caller_ptr, *dest_ptr, *next_ptr;
    std::size_t vb;
    std::size_t vlo, vhi;
    std::size_t len;

    /* Original MINIX restricted user sends to FS/MM only. Removed: microkernel
     * servers should be reachable by any process via IPC. The P_SLOT_FREE check
     * below still validates the destination exists. */
    caller_ptr = proc_addr(caller);
    dest_ptr = proc_addr(dest);
    if (dest_ptr->p_flags & static_cast<int>(P_SLOT_FREE))
        return static_cast<int>(ErrorCode::E_BAD_DEST);

    len = caller_ptr->p_map[D].mem_len;
    vb = reinterpret_cast<std::size_t>(m_ptr);
    vlo = vb >> CLICK_SHIFT;
    vhi = (vb + sizeof(message) - 1) >> CLICK_SHIFT;
    if (vhi < vlo || vhi - caller_ptr->p_map[D].mem_vir >= len)
        return static_cast<int>(ErrorCode::E_BAD_ADDR);

    if ((dest_ptr->p_flags & static_cast<int>(RECEIVING)) &&
        (dest_ptr->p_getfrom == ANY || dest_ptr->p_getfrom == caller)) {
        cp_mess(caller, 0, m_ptr, 0, dest_ptr->p_messbuf);
        dest_ptr->p_flags &= ~static_cast<int>(RECEIVING);
        if (dest_ptr->p_flags == 0)
            ready(dest_ptr);
    } else {
        if (caller == HARDWARE)
            return static_cast<int>(ErrorCode::E_OVERRUN);
        caller_ptr->p_messbuf = m_ptr;
        caller_ptr->p_flags |= static_cast<int>(SENDING);
        unready(caller_ptr);

        if ((next_ptr = dest_ptr->p_callerq) == nullptr) {
            dest_ptr->p_callerq = caller_ptr;
        } else {
            while (next_ptr->p_sendlink != nullptr)
                next_ptr = next_ptr->p_sendlink;
            next_ptr->p_sendlink = caller_ptr;
        }
        caller_ptr->p_sendlink = nullptr;
    }
    return (OK);
}

/*===========================================================================*
 *				mini_rec				     *
 *===========================================================================*/
extern "C" int mini_rec(int caller, int src, message *m_ptr) noexcept {
    struct proc *caller_ptr, *sender_ptr, *prev_ptr = nullptr;
    int sender;

    caller_ptr = proc_addr(caller);
    sender_ptr = caller_ptr->p_callerq;
    while (sender_ptr != nullptr) {
        sender = static_cast<int>(sender_ptr - proc - NR_TASKS);
        if (src == ANY || src == sender) {
            cp_mess(sender, 0, sender_ptr->p_messbuf, 0, m_ptr);
            sender_ptr->p_flags &= ~static_cast<int>(SENDING);
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

    caller_ptr->p_getfrom = src;
    caller_ptr->p_messbuf = m_ptr;
    caller_ptr->p_flags |= static_cast<int>(RECEIVING);
    unready(caller_ptr);

    if (sig_procs > 0 && caller == MM_PROC_NR && src == ANY)
        inform(MM_PROC_NR);
    return (OK);
}

/*===========================================================================*
 *				ipc_send				     *
 *===========================================================================*/
extern "C" int ipc_send(int dest, message *m_ptr) noexcept {
    return mini_send(cur_proc, dest, m_ptr);
}

/*===========================================================================*
 *				ipc_receive				     *
 *===========================================================================*/
extern "C" int ipc_receive(int src, message *m_ptr) noexcept {
    return mini_rec(cur_proc, src, m_ptr);
}

/*===========================================================================*
 *				pick_proc				     *
 *===========================================================================*/
extern "C" void pick_proc() noexcept {
    int q;
    for (q = 0; q < SCHED_QUEUES; q++) {
        if (rdy_head[current_cpu][q] != nullptr)
            break;
    }
    if (q == SCHED_QUEUES) q = PRI_USER; 

    prev_proc = cur_proc;
    if (rdy_head[current_cpu][q] != nullptr) {
        cur_proc = static_cast<int>(rdy_head[current_cpu][q] - proc - NR_TASKS);
        proc_ptr = rdy_head[current_cpu][q];
        if (cur_proc >= LOW_USER)
            bill_ptr = proc_ptr;
    } else {
        cur_proc = IDLE;
        proc_ptr = proc_addr(HARDWARE);
        bill_ptr = proc_ptr;
    }
}

/*===========================================================================*
 *				ready					     *
 *===========================================================================*/
extern "C" void ready(struct proc *rp) noexcept {
    int q;
    int cpu = rp->p_cpu;

    lock();
    q = rp->p_priority;
    if (q < 0) q = 0;
    if (q >= SCHED_QUEUES) q = SCHED_QUEUES - 1;

    if (rdy_head[cpu][q] == nullptr)
        rdy_head[cpu][q] = rp;
    else
        rdy_tail[cpu][q]->p_nextready = rp;
    rdy_tail[cpu][q] = rp;
    rp->p_nextready = nullptr;
    restore();
}

/*===========================================================================*
 *				unready					     *
 *===========================================================================*/
extern "C" void unready(struct proc *rp) noexcept {
    struct proc *xp;
    int q;
    int cpu = rp->p_cpu;

    lock();
    q = rp->p_priority;
    if (q < 0) q = 0;
    if (q >= SCHED_QUEUES) q = SCHED_QUEUES - 1;
    if ((xp = rdy_head[cpu][q]) == nullptr) {
        restore();
        return;
    }
    if (xp == rp) {
        rdy_head[cpu][q] = xp->p_nextready;
        pick_proc();
    } else {
        while (xp->p_nextready != rp) {
            if ((xp = xp->p_nextready) == nullptr) {
                restore();
                return;
            }
        }
        xp->p_nextready = xp->p_nextready->p_nextready;
        while (xp->p_nextready != nullptr)
            xp = xp->p_nextready;
        rdy_tail[cpu][q] = xp;
    }
    restore();
}

/*===========================================================================*
 *				kernel_sched					     *
 *===========================================================================*/
extern "C" void kernel_sched() noexcept {
    lock();
    int q = proc_ptr->p_priority;
    int cpu = proc_ptr->p_cpu;
    if (rdy_head[cpu][q] == nullptr || rdy_head[cpu][q]->p_nextready == nullptr) {
        restore();
        return;
    }
    rdy_tail[cpu][q]->p_nextready = rdy_head[cpu][q];
    rdy_tail[cpu][q] = rdy_head[cpu][q];
    rdy_head[cpu][q] = rdy_head[cpu][q]->p_nextready;
    rdy_tail[cpu][q]->p_nextready = nullptr;
    pick_proc();
    restore();
}
