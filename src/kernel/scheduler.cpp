/**
 * @file scheduler.cpp
 * @brief Preemptive scheduler for XINIM microkernel.
 *
 * This module provides the top-level scheduler entry points. The actual
 * ready-queue management is in proc.cpp (MINIX heritage: pick_proc(),
 * ready(), unready()). The higher-level blocking/yield/deadlock-detection
 * API lives in schedule.cpp (sched::Scheduler).
 *
 * This file bridges the two: schedule() calls pick_proc(), and
 * start_scheduler() runs the non-returning dispatch loop.
 */

#include <stdint.h>
#include "early/serial_16550.hpp"
#include "proc.hpp"
#include "scheduler.hpp"

extern xinim::early::Serial16550 early_serial;

// From proc.cpp -- MINIX heritage ready-queue dispatcher
extern "C" void pick_proc() noexcept;

namespace xinim::kernel {

void start_scheduler() {
    early_serial.write("[SCHEDULER] Starting...\n");
    while (true) {
        schedule();
        asm volatile("hlt");
    }
}

void schedule() {
    // Delegate to the MINIX heritage pick_proc() which walks the
    // priority-based ready queues and sets proc_ptr/cur_proc.
    pick_proc();
}

void print_scheduler_stats() {
    early_serial.write("[SCHEDULER] Stats called\n");
}

} // namespace xinim::kernel
