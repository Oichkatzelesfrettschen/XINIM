/**
 * @file process_lifecycle.cpp
 * @brief Process exit and wait implementation.
 *
 * See process_lifecycle.hpp for design rationale.
 */

#include "process_lifecycle.hpp"

#include "heap.hpp"
#include "unified_scheduler.hpp"
#ifdef XINIM_ARCH_X86_64
#include "arch/x86_64/user_address_space.hpp"
#endif

namespace xinim::kernel {

    void process_exit(xinim::pid_t pid, int status) {
        ProcessControlBlock *pcb = g_unified_scheduler.find_by_pid(pid);
        if (!pcb)
            return;

        // Mark as zombie
        pcb->state = ProcessState::ZOMBIE;
        pcb->exit_status = status;
        pcb->has_exited = true;

        // Dequeue from scheduler (if still in a run queue)
        g_unified_scheduler.dequeue(pcb);

        // Free user stack
        if (pcb->stack_base) {
            heap_free(pcb->stack_base);
            pcb->stack_base = nullptr;
            pcb->stack_size = 0;
        }

        // Free kernel stack
        if (pcb->kernel_stack_base) {
            heap_free(pcb->kernel_stack_base);
            pcb->kernel_stack_base = nullptr;
            pcb->kernel_stack_size = 0;
        }

#ifdef XINIM_ARCH_X86_64
        xinim::kernel::x86_64::UserAddressSpace address_space{pcb->address_space_root};
        xinim::kernel::x86_64::destroy_user_address_space(address_space);
        pcb->address_space_root = 0U;
        pcb->context.cr3 = 0U;
#endif

        // If the exiting process is the current one, pick next without re-enqueuing
        if (g_unified_scheduler.current() == pcb) {
            // Don't use yield() -- that would re-enqueue and overwrite ZOMBIE state.
            // Just pick the next process directly.
            g_unified_scheduler.pick_next();
        }

        // Notify parent if it is waiting
        ProcessControlBlock *parent = g_unified_scheduler.find_by_pid(pcb->parent_pid);
        if (parent && parent->state == ProcessState::BLOCKED &&
            parent->blocked_on == BlockReason::WAIT_CHILD) {
            g_unified_scheduler.unblock(parent->pid);
        }
    }

    xinim::pid_t process_wait(xinim::pid_t parent_pid, xinim::pid_t child_pid, int *status_out) {
        ProcessControlBlock *parent = g_unified_scheduler.find_by_pid(parent_pid);
        if (!parent)
            return -1;

        // Scan for zombie children
        bool has_children = false;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            ProcessControlBlock *child =
                g_unified_scheduler.find_by_pid(static_cast<xinim::pid_t>(i));
            if (!child || child->parent_pid != parent_pid)
                continue;

            has_children = true;

            if (child_pid > 0 && child->pid != child_pid)
                continue;

            if (child->state == ProcessState::ZOMBIE) {
                // Found a zombie child -- reap it
                if (status_out) {
                    *status_out = child->exit_status;
                }
                xinim::pid_t reaped_pid = child->pid;
                child->state = ProcessState::DEAD;
                child->has_been_waited = true;
                return reaped_pid;
            }
        }

        if (!has_children)
            return -1; // No children at all

        // No zombie found -- block parent
        g_unified_scheduler.block(parent, BlockReason::WAIT_CHILD, -1);
        return 0; // Will be retried when unblocked
    }

} // namespace xinim::kernel
