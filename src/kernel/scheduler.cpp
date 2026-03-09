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
#ifdef XINIM_ARCH_X86_64
#include "arch/x86_64/tss.hpp"
#endif

extern xinim::early::Serial16550 early_serial;

// From proc.cpp -- still called for MINIX heritage IPC (mini_send/mini_rec)
extern "C" void pick_proc() noexcept;
extern "C" [[noreturn]] void load_context(xinim::kernel::CpuContext* context);
extern "C" [[noreturn]] void load_context_ring3(xinim::kernel::CpuContext* context);

namespace xinim::kernel {

void start_scheduler() {
    early_serial.write("[SCHEDULER] Starting unified scheduler...\n");

    ProcessControlBlock* next = g_unified_scheduler.pick_next();
    if (!next) {
        early_serial.write("[SCHEDULER] No runnable process, idling\n");
        while (true) {
            asm volatile("hlt");
        }
    }

    early_serial.write("[SCHEDULER] First handoff to PID ");
    char pid_buf[16];
    int pid = next->pid;
    int used = 0;
    if (pid == 0) {
        pid_buf[used++] = '0';
    } else {
        char reverse[16];
        int reverse_used = 0;
        while (pid > 0 && reverse_used < static_cast<int>(sizeof(reverse))) {
            reverse[reverse_used++] = static_cast<char>('0' + (pid % 10));
            pid /= 10;
        }
        while (reverse_used > 0) {
            pid_buf[used++] = reverse[--reverse_used];
        }
    }
    pid_buf[used] = '\0';
    early_serial.write(pid_buf);
    early_serial.write(" (");
    early_serial.write(next->name != nullptr ? next->name : "unnamed");
    early_serial.write(")\n");

#ifdef XINIM_ARCH_X86_64
    if (next->kernel_rsp != 0U) {
        xinim::kernel::set_kernel_stack(next->kernel_rsp);
    }
#endif

    if (next->context.cs == 0x1B || next->context.cs == 0x23) {
        load_context_ring3(&next->context);
    }
    load_context(&next->context);
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
