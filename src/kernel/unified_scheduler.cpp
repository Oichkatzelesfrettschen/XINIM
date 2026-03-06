/**
 * @file unified_scheduler.cpp
 * @brief O(1) bitmap-based unified scheduler implementation.
 *
 * See unified_scheduler.hpp for design rationale.
 */

#include "unified_scheduler.hpp"

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

void UnifiedScheduler::add_process(ProcessControlBlock* pcb) {
    if (!pcb) return;
    int idx = pcb->pid;
    if (idx < 0 || idx >= MAX_PROCESSES) return;

    proc_table_[idx] = pcb;

    if (pcb->state == ProcessState::READY) {
        enqueue(pcb);
    }
}

void UnifiedScheduler::enqueue(ProcessControlBlock* pcb) {
    if (!pcb) return;

    uint32_t prio = pcb->priority;
    if (prio >= static_cast<uint32_t>(NUM_PRIORITIES)) {
        prio = static_cast<uint32_t>(NUM_PRIORITIES) - 1;
    }

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

void UnifiedScheduler::dequeue(ProcessControlBlock* pcb) {
    if (!pcb) return;

    uint32_t prio = pcb->priority;
    if (prio >= static_cast<uint32_t>(NUM_PRIORITIES)) {
        prio = static_cast<uint32_t>(NUM_PRIORITIES) - 1;
    }

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

ProcessControlBlock* UnifiedScheduler::pick_next() {
    if (priority_bitmap_ == 0) return nullptr;

    // O(1): find lowest set bit (highest priority = lowest number)
    int prio = __builtin_ctzll(priority_bitmap_);

    ProcessControlBlock* next = run_queue_head_[prio];
    if (next) {
        dequeue(next);
        next->state = ProcessState::RUNNING;
        current_ = next;

        // Set quantum based on priority
        uint32_t q = quantum_for_priority(static_cast<uint32_t>(prio));
        ticks_remaining_ = q;
    }

    return next;
}

bool UnifiedScheduler::block(ProcessControlBlock* pcb, BlockReason reason,
                             xinim::pid_t target) {
    if (!pcb) return false;

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
    if (pid < 0 || pid >= MAX_PROCESSES) return;

    ProcessControlBlock* pcb = proc_table_[pid];
    if (!pcb || pcb->state != ProcessState::BLOCKED) return;

    // Remove wait-for edge
    if (pcb->ipc_wait_source >= 0 && pcb->ipc_wait_source < MAX_PROCESSES) {
        wait_graph_.remove_edge(pcb->pid, pcb->ipc_wait_source);
    }

    pcb->blocked_on = BlockReason::NONE;
    pcb->ipc_wait_source = -1;

    enqueue(pcb);
}

void UnifiedScheduler::yield() {
    if (current_) {
        ProcessControlBlock* prev = current_;
        current_ = nullptr;
        enqueue(prev);
    }
    pick_next();
}

void UnifiedScheduler::yield_to(xinim::pid_t pid) {
    if (pid < 0 || pid >= MAX_PROCESSES) return;

    ProcessControlBlock* target = proc_table_[pid];
    if (!target) return;

    // Re-enqueue current
    if (current_) {
        ProcessControlBlock* prev = current_;
        current_ = nullptr;
        enqueue(prev);
    }

    // Dequeue target if in a run queue
    if (target->state == ProcessState::READY) {
        dequeue(target);
    }

    target->state = ProcessState::RUNNING;
    current_ = target;
    ticks_remaining_ = quantum_for_priority(target->priority);
}

void UnifiedScheduler::timer_tick() {
    tick_count_++;

    if (!current_) return;

    current_->total_ticks++;

    // System tasks (quantum=0) are not preempted
    uint32_t q = quantum_for_priority(current_->priority);
    if (q == 0) return;

    if (ticks_remaining_ > 0) {
        ticks_remaining_--;
    }

    if (ticks_remaining_ == 0) {
        yield(); // Quantum expired -- round-robin within priority
    }
}

ProcessControlBlock* UnifiedScheduler::find_by_pid(xinim::pid_t pid) const {
    if (pid < 0 || pid >= MAX_PROCESSES) return nullptr;
    return proc_table_[pid];
}

} // namespace xinim::kernel
