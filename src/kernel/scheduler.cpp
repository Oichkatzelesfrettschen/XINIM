/**
 * @file scheduler.cpp
 * @brief Preemptive scheduler for XINIM microkernel.
 *
 * v1.2.0: Bridges the scheduler.hpp API to the unified scheduler.
 * The UnifiedScheduler (unified_scheduler.cpp) is now the single source
 * of truth for process scheduling. This file provides the legacy API
 * surface that the rest of the kernel calls.
 */

#include <stdint.h>
#include "early/serial_16550.hpp"
#include "proc.hpp"
#include "scheduler.hpp"
#include "unified_scheduler.hpp"

extern xinim::early::Serial16550 early_serial;

// From proc.cpp -- still called for MINIX heritage IPC (mini_send/mini_rec)
extern "C" void pick_proc() noexcept;

namespace xinim::kernel {

void start_scheduler() {
    early_serial.write("[SCHEDULER] Starting unified scheduler...\n");
    while (true) {
        schedule();
        asm volatile("hlt");
    }
}

void schedule() {
    // Delegate to MINIX heritage pick_proc for legacy IPC path
    pick_proc();
}

void initialize_scheduler() {
    // Unified scheduler is statically initialized
}

void scheduler_add_process(ProcessControlBlock* pcb) {
    g_unified_scheduler.add_process(pcb);
}

ProcessControlBlock* get_current_process() {
    return g_unified_scheduler.current();
}

void block_current_process(BlockReason reason, xinim::pid_t wait_source) {
    auto* cur = g_unified_scheduler.current();
    if (cur) {
        g_unified_scheduler.block(cur, reason, wait_source);
    }
}

void unblock_process(ProcessControlBlock* pcb) {
    if (pcb) {
        g_unified_scheduler.unblock(pcb->pid);
    }
}

ProcessControlBlock* find_process_by_pid(xinim::pid_t pid) {
    return g_unified_scheduler.find_by_pid(pid);
}

uint64_t get_tick_count() {
    return g_unified_scheduler.tick_count();
}

void print_scheduler_stats() {
    early_serial.write("[SCHEDULER] Stats called\n");
}

} // namespace xinim::kernel
