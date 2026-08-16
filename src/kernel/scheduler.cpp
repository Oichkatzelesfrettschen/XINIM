/**
 * @file scheduler.cpp
 * @brief Runnable-process selection and initial context handoff bridge.
 *
 * UnifiedScheduler owns runnable state and selection. This file exposes the
 * kernel scheduler API and performs the initial architecture context handoff.
 */

#include "scheduler.hpp"

#include "early/serial_16550.hpp"
#include "panic.hpp"
#include "proc.hpp"
#include "unified_scheduler.hpp"

#include <stdint.h>
#ifdef XINIM_ARCH_X86_64
#include "arch/x86_64/context_restore.hpp"
#include "arch/x86_64/syscall_init.hpp"
#include "arch/x86_64/tss.hpp"

#include <xinim/abi/x86_segment_selectors.h>
#endif

extern xinim::early::Serial16550 early_serial;

// From proc.cpp -- still called for MINIX heritage IPC (mini_send/mini_rec)
extern "C" void pick_proc() noexcept;

namespace xinim::kernel {

    void start_scheduler() {
        early_serial.write("[SCHEDULER] Starting unified scheduler...\n");

        ProcessControlBlock *next = g_unified_scheduler.pick_next();
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
            xinim::kernel::set_syscall_kernel_stack(next->kernel_rsp);
        }
#endif

        if (next->context.cs == XINIM_X86_USER_CS_SELECTOR) {
            if (next->context.ss != XINIM_X86_USER_DS_SELECTOR) {
                kpanic("user context has an invalid stack selector");
            }
            load_context_ring3(&next->context);
        }
        if (next->context.cs != XINIM_X86_KERNEL_CS_SELECTOR ||
            next->context.ss != XINIM_X86_KERNEL_DS_SELECTOR) {
            kpanic("kernel context has invalid code or stack selectors");
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

    void scheduler_add_process(ProcessControlBlock *pcb) {
        g_unified_scheduler.add_process(pcb);
    }

    ProcessControlBlock *get_current_process() {
        return g_unified_scheduler.current();
    }

    void block_current_process(BlockReason reason, xinim::pid_t wait_source) {
        auto *cur = g_unified_scheduler.current();
        if (cur) {
            g_unified_scheduler.block(cur, reason, wait_source);
        }
    }

    void unblock_process(ProcessControlBlock *pcb) {
        if (pcb) {
            g_unified_scheduler.unblock(pcb->pid);
        }
    }

    ProcessControlBlock *find_process_by_pid(xinim::pid_t pid) {
        return g_unified_scheduler.find_by_pid(pid);
    }

    uint64_t get_tick_count() {
        return g_unified_scheduler.tick_count();
    }

    void print_scheduler_stats() {
        early_serial.write("[SCHEDULER] Stats called\n");
    }

} // namespace xinim::kernel
