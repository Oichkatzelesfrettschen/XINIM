/**
 * @file unified_scheduler.cpp
 * @brief O(1) bitmap-based unified scheduler implementation.
 *
 * See unified_scheduler.hpp for design rationale.
 */

#include "unified_scheduler.hpp"

#include "signal.hpp"

namespace xinim::kernel {

    UnifiedScheduler g_unified_scheduler;

    UnifiedScheduler::UnifiedScheduler() {
        for (int i = 0; i < NUM_PRIORITIES; i++) {
            run_queue_head_[i] = nullptr;
            run_queue_tail_[i] = nullptr;
        }
        for (int i = 0; i < MAX_PROCESSES; i++) {
            proc_table_[i] = nullptr;
        }
    }

    uint32_t UnifiedScheduler::effective_quantum(const ProcessControlBlock *pcb) const {
        if (!pcb)
            return 0;
        if (pcb->quantum_ticks != 0) {
            return pcb->quantum_ticks;
        }
        return sched_policy::quantum_for_priority(pcb->priority);
    }

    void UnifiedScheduler::add_process(ProcessControlBlock *pcb) {
        if (!pcb)
            return;
        int idx = pcb->pid;
        if (idx < 0 || idx >= MAX_PROCESSES)
            return;

        if (pcb->base_priority == 0U && pcb->priority != 0U) {
            pcb->base_priority = pcb->priority;
        }
        if (pcb->scheduler_domain == 0U) {
            pcb->scheduler_domain = 1U;
        }

        proc_table_[idx] = pcb;

        if (pcb->state == ProcessState::READY) {
            enqueue(pcb);
        }
    }

    bool UnifiedScheduler::remove_process(ProcessControlBlock *pcb) {
        if (!pcb || pcb == current_ || pcb->pid < 0 || pcb->pid >= MAX_PROCESSES ||
            proc_table_[pcb->pid] != pcb) {
            return false;
        }

        if (pcb->state == ProcessState::READY) {
            dequeue(pcb);
        }
        wait_graph_.remove_node(pcb->pid);
        proc_table_[pcb->pid] = nullptr;
        pcb->state = ProcessState::DEAD;
        pcb->next = nullptr;
        pcb->prev = nullptr;
        return true;
    }

    void UnifiedScheduler::enqueue(ProcessControlBlock *pcb) {
        if (!pcb)
            return;

        uint32_t prio = sched_policy::clamp_priority(pcb->priority);

        pcb->state = ProcessState::READY;
        pcb->next = nullptr;
        pcb->prev = run_queue_tail_[prio];

        if (run_queue_tail_[prio]) {
            run_queue_tail_[prio]->next = pcb;
        } else {
            run_queue_head_[prio] = pcb;
        }
        run_queue_tail_[prio] = pcb;

        priority_bitmap_ |= (1ULL << prio);
    }

    void UnifiedScheduler::dequeue(ProcessControlBlock *pcb) {
        if (!pcb)
            return;

        uint32_t prio = sched_policy::clamp_priority(pcb->priority);

        // Unlink from doubly-linked list
        if (pcb->prev) {
            pcb->prev->next = pcb->next;
        } else {
            run_queue_head_[prio] = pcb->next;
        }

        if (pcb->next) {
            pcb->next->prev = pcb->prev;
        } else {
            run_queue_tail_[prio] = pcb->prev;
        }

        pcb->next = nullptr;
        pcb->prev = nullptr;

        // Clear bitmap bit if queue is now empty
        if (!run_queue_head_[prio]) {
            priority_bitmap_ &= ~(1ULL << prio);
        }
    }

    ProcessControlBlock *UnifiedScheduler::pick_next() {
        if (priority_bitmap_ == 0)
            return nullptr;

        // O(1): find lowest set bit (highest priority = lowest number)
        int prio = __builtin_ctzll(priority_bitmap_);

        ProcessControlBlock *next = run_queue_head_[prio];
        if (next) {
            dequeue(next);
            next->state = ProcessState::RUNNING;
            current_ = next;

            ticks_remaining_ = effective_quantum(next);
        }

        return next;
    }

    bool UnifiedScheduler::block(ProcessControlBlock *pcb, BlockReason reason,
                                 xinim::pid_t target) {
        if (!pcb)
            return false;

        // Check for deadlock before blocking
        if (target >= 0 && target < MAX_PROCESSES) {
            wait_graph_.add_edge(pcb->pid, target);
            if (wait_graph_.is_in_cycle(pcb->pid)) {
                wait_graph_.remove_edge(pcb->pid, target);
                return false; // Would deadlock
            }
        }

        // Remove from run queue if currently enqueued
        if (pcb->state == ProcessState::READY) {
            dequeue(pcb);
        }

        pcb->state = ProcessState::BLOCKED;
        pcb->blocked_on = reason;
        pcb->ipc_wait_source = target;

        if (current_ == pcb) {
            current_ = nullptr;
        }

        return true;
    }

    void UnifiedScheduler::unblock(xinim::pid_t pid) {
        if (pid < 0 || pid >= MAX_PROCESSES)
            return;

        ProcessControlBlock *pcb = proc_table_[pid];
        if (!pcb || pcb->state != ProcessState::BLOCKED)
            return;

        // Remove wait-for edge
        if (pcb->ipc_wait_source >= 0 && pcb->ipc_wait_source < MAX_PROCESSES) {
            wait_graph_.remove_edge(pcb->pid, pcb->ipc_wait_source);
        }

        pcb->blocked_on = BlockReason::NONE;
        pcb->ipc_wait_source = -1;

        enqueue(pcb);
    }

    void UnifiedScheduler::stop(ProcessControlBlock *pcb) {
        if (pcb == nullptr) {
            return;
        }
        if (pcb->state == ProcessState::READY) {
            dequeue(pcb);
        }
        if (current_ == pcb) {
            current_ = nullptr;
        }
        wait_graph_.remove_node(pcb->pid);
        pcb->blocked_on = BlockReason::NONE;
        pcb->ipc_wait_source = -1;
        pcb->state = ProcessState::STOPPED;
        pcb->next = nullptr;
        pcb->prev = nullptr;
    }

    void UnifiedScheduler::yield() {
        if (current_) {
            ProcessControlBlock *prev = current_;
            current_ = nullptr;
            enqueue(prev);
        }
        pick_next();
    }

    void UnifiedScheduler::yield_to(xinim::pid_t pid) {
        if (pid < 0 || pid >= MAX_PROCESSES)
            return;

        ProcessControlBlock *target = proc_table_[pid];
        if (!target)
            return;

        // Re-enqueue current
        if (current_) {
            ProcessControlBlock *prev = current_;
            current_ = nullptr;
            enqueue(prev);
        }

        // Dequeue target if in a run queue
        if (target->state == ProcessState::READY) {
            dequeue(target);
        }

        target->state = ProcessState::RUNNING;
        current_ = target;
        ticks_remaining_ = effective_quantum(target);
    }

    void UnifiedScheduler::rebalance_priorities() {
        for (int index = 0; index < MAX_PROCESSES; ++index) {
            ProcessControlBlock *pcb = proc_table_[index];
            if (!pcb)
                continue;
            if (pcb->priority <= pcb->base_priority)
                continue;

            const uint32_t old_priority = pcb->priority;
            if (pcb->state == ProcessState::READY) {
                dequeue(pcb);
            }

            pcb->priority = sched_policy::rebalance_toward_base(pcb->priority, pcb->base_priority);

            if (pcb->state == ProcessState::READY) {
                enqueue(pcb);
            } else if (pcb == current_ && ticks_remaining_ > effective_quantum(pcb)) {
                ticks_remaining_ = effective_quantum(pcb);
            }

            (void) old_priority;
        }
    }

    void UnifiedScheduler::timer_tick(bool allow_preemption) {
        tick_count_++;

        for (int process_id = 1; process_id < MAX_PROCESSES; ++process_id) {
            ProcessControlBlock *process = proc_table_[process_id];
            if (process == nullptr || process->state == ProcessState::DEAD ||
                process->state == ProcessState::ZOMBIE) {
                continue;
            }
            if (process->alarm_deadline_tick != 0U && tick_count_ >= process->alarm_deadline_tick) {
                process->alarm_deadline_tick = 0U;
                static_cast<void>(send_signal(process, SIGALRM));
            }
            if (process->state == ProcessState::BLOCKED &&
                (process->blocked_on == BlockReason::TIMER ||
                 process->blocked_on == BlockReason::SELECT) &&
                process->wake_deadline_tick != 0U && tick_count_ >= process->wake_deadline_tick) {
                process->wake_deadline_tick = 0U;
                if (process->blocked_on == BlockReason::TIMER) {
                    process->sleep_remaining_address = 0U;
                    process->context.rax = 0U;
                }
                unblock(process->pid);
            }
        }

        if ((tick_count_ % PRIORITY_REBALANCE_PERIOD_TICKS) == 0U) {
            rebalance_priorities();
        }

        if (!current_)
            return;

        current_->total_ticks++;

        // System tasks (quantum=0) are not preempted
        uint32_t q = effective_quantum(current_);
        if (q == 0)
            return;

        if (ticks_remaining_ > 0) {
            ticks_remaining_--;
        }

        if (ticks_remaining_ == 0) {
            if (!allow_preemption) {
                return;
            }
            if (sched_policy::should_demote_on_quantum_expiry(current_->priority)) {
                current_->priority = sched_policy::demote_priority(current_->priority);
            }
            yield(); // Quantum expired -- round-robin within priority
        }
    }

    ProcessControlBlock *UnifiedScheduler::find_by_pid(xinim::pid_t pid) const {
        if (pid < 0 || pid >= MAX_PROCESSES)
            return nullptr;
        return proc_table_[pid];
    }

} // namespace xinim::kernel
