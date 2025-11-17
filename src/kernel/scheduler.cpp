/**
 * @file scheduler.cpp
 * @brief Preemptive scheduler for XINIM microkernel
 *
 * Implements round-robin scheduling with context switching.
 * Called from timer interrupt handler ~100 times per second.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "scheduler.hpp"
#include "pcb.hpp"  // Week 8: Consolidated Process Control Block
#include "context.hpp"
#include "arch/x86_64/tss.hpp"  // Week 8 Phase 3: For set_kernel_stack()
#include "early/serial_16550.hpp"
#include <cstdio>
#include <cstring>

// External references
extern xinim::early::Serial16550 early_serial;

// Assembly functions
extern "C" void context_switch(xinim::kernel::CpuContext* current,
                               xinim::kernel::CpuContext* next);
extern "C" void load_context(xinim::kernel::CpuContext* ctx);
extern "C" [[noreturn]] void load_context_ring3(xinim::kernel::CpuContext* ctx);

namespace xinim::kernel {

// Week 8: PCB, ProcessState, BlockReason now defined in pcb.hpp (consolidated)

// ============================================================================
// Global Scheduler State
// ============================================================================

static ProcessControlBlock* g_ready_queue_head = nullptr;
static ProcessControlBlock* g_ready_queue_tail = nullptr;
static ProcessControlBlock* g_current_process = nullptr;
static ProcessControlBlock* g_idle_process = nullptr;

static uint64_t g_ticks = 0;  // Global tick counter

// ============================================================================
// Queue Management Functions
// ============================================================================

/**
 * @brief Add process to ready queue (tail insertion for fairness)
 */
static void ready_queue_add(ProcessControlBlock* pcb) {
    pcb->state = ProcessState::READY;
    pcb->next = nullptr;
    pcb->prev = g_ready_queue_tail;

    if (g_ready_queue_tail) {
        g_ready_queue_tail->next = pcb;
    } else {
        g_ready_queue_head = pcb;
    }

    g_ready_queue_tail = pcb;
}

/**
 * @brief Remove process from ready queue
 */
static void ready_queue_remove(ProcessControlBlock* pcb) {
    if (pcb->prev) {
        pcb->prev->next = pcb->next;
    } else {
        g_ready_queue_head = pcb->next;
    }

    if (pcb->next) {
        pcb->next->prev = pcb->prev;
    } else {
        g_ready_queue_tail = pcb->prev;
    }

    pcb->next = pcb->prev = nullptr;
}

/**
 * @brief Get next process from ready queue (head removal for FIFO)
 */
static ProcessControlBlock* ready_queue_pop() {
    if (!g_ready_queue_head) {
        return nullptr;
    }

    ProcessControlBlock* pcb = g_ready_queue_head;
    ready_queue_remove(pcb);
    return pcb;
}

// ============================================================================
// Scheduler Core
// ============================================================================

/**
 * @brief Pick next process to run (round-robin)
 *
 * Simple round-robin scheduling for Week 8.
 * Week 9 will add priority-based scheduling.
 *
 * @return Next process to run, or idle process if none ready
 */
static ProcessControlBlock* pick_next_process() {
    // Get first ready process from queue
    ProcessControlBlock* next = ready_queue_pop();

    if (next) {
        return next;
    }

    // No ready processes - return idle
    return g_idle_process;
}

/**
 * @brief Perform context switch
 *
 * This is the heart of the scheduler. It saves the current process's
 * context and restores the next process's context.
 *
 * @param next Process to switch to
 */
static void switch_to(ProcessControlBlock* next) {
    ProcessControlBlock* current = g_current_process;

    if (current == next) {
        return;  // Already running this process
    }

    // Update states
    if (current && current->state == ProcessState::RUNNING) {
        // Current process was running, put it back in ready queue
        ready_queue_add(current);
    }

    next->state = ProcessState::RUNNING;
    next->time_quantum_start = g_ticks;

    // Week 8 Phase 3: Set kernel stack for next process
    // When interrupt occurs in Ring 3, CPU will use this stack
    set_kernel_stack(next->kernel_rsp);

    // Update current process pointer
    g_current_process = next;

    // Perform actual context switch
    if (current) {
        context_switch(&current->context, &next->context);
    } else {
        // First time - no previous context to save
        // Check if Ring 3 or Ring 0
        if (next->context.cs == 0x1B) {
            // Ring 3 process - use iretq transition
            load_context_ring3(&next->context);
        } else {
            // Ring 0 process - use normal load
            load_context(&next->context);
        }
    }
}

/**
 * @brief Main scheduling function
 *
 * Called from timer interrupt handler. Picks next process and switches to it.
 */
void schedule() {
    // Increment tick counter
    g_ticks++;

    // Pick next process
    ProcessControlBlock* next = pick_next_process();

    if (!next) {
        // No processes at all - something is very wrong
        early_serial.write("[FATAL] No processes to schedule!\n");
        for(;;) { __asm__ volatile ("hlt"); }
    }

    // Switch to next process
    switch_to(next);
}

// ============================================================================
// Process Management Functions (called from spawn, IPC, etc.)
// ============================================================================

/**
 * @brief Add process to scheduler
 *
 * Called from spawn_server() and other process creation functions.
 */
void scheduler_add_process(ProcessControlBlock* pcb) {
    ready_queue_add(pcb);
}

/**
 * @brief Get current running process
 */
ProcessControlBlock* get_current_process() {
    return g_current_process;
}

/**
 * @brief Block current process
 *
 * Called when process waits for IPC, I/O, etc.
 */
void block_current_process(BlockReason reason, pid_t wait_source) {
    if (!g_current_process) {
        return;
    }

    g_current_process->state = ProcessState::BLOCKED;
    g_current_process->blocked_on = reason;
    g_current_process->ipc_wait_source = wait_source;

    // Force reschedule
    schedule();
}

/**
 * @brief Unblock process
 *
 * Called when IPC message arrives, I/O completes, etc.
 */
void unblock_process(ProcessControlBlock* pcb) {
    if (pcb->state != ProcessState::BLOCKED) {
        return;
    }

    pcb->state = ProcessState::READY;
    pcb->blocked_on = BlockReason::NONE;
    ready_queue_add(pcb);
}

/**
 * @brief Find process by PID
 */
ProcessControlBlock* find_process_by_pid(pid_t pid) {
    // Search ready queue
    ProcessControlBlock* current = g_ready_queue_head;
    while (current) {
        if (current->pid == pid) {
            return current;
        }
        current = current->next;
    }

    // Check current process
    if (g_current_process && g_current_process->pid == pid) {
        return g_current_process;
    }

    // TODO: Search blocked queue (not implemented yet)

    return nullptr;
}

// ============================================================================
// Scheduler Initialization
// ============================================================================

/**
 * @brief Initialize scheduler
 *
 * Called during kernel initialization, before any processes are created.
 */
void initialize_scheduler() {
    g_ready_queue_head = nullptr;
    g_ready_queue_tail = nullptr;
    g_current_process = nullptr;
    g_ticks = 0;

    // TODO: Create idle process
    // For Week 8, we'll just halt if no processes are ready

    early_serial.write("[SCHEDULER] Initialized (round-robin, preemptive)\n");
}

/**
 * @brief Start scheduler and enter main loop
 *
 * This function never returns. It enables interrupts and enters
 * an idle loop. The timer interrupt will handle scheduling.
 */
[[noreturn]] void start_scheduler() {
    early_serial.write("[SCHEDULER] Starting preemptive scheduling...\n");

    // Pick first process
    ProcessControlBlock* first = ready_queue_pop();
    if (!first) {
        early_serial.write("[FATAL] No processes to schedule on startup!\n");
        for(;;) { __asm__ volatile ("hlt"); }
    }

    first->state = ProcessState::RUNNING;
    g_current_process = first;

    // Week 8 Phase 3: Set kernel stack before starting Ring 3 process
    set_kernel_stack(first->kernel_rsp);

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[SCHEDULER] Starting first process: %s (PID %d) in Ring %d\n",
             first->name, first->pid,
             (first->context.cs == 0x1B) ? 3 : 0);
    early_serial.write(buffer);

    // Enable interrupts and load first process
    // This will start the first process and begin timer-based preemption
    __asm__ volatile ("sti");  // Enable interrupts

    // Check if Ring 3 or Ring 0
    if (first->context.cs == 0x1B) {
        // Ring 3 process - use iretq transition
        load_context_ring3(&first->context);
    } else {
        // Ring 0 process - use normal load
        load_context(&first->context);
    }

    // Never returns
}

// ============================================================================
// Statistics and Debugging
// ============================================================================

/**
 * @brief Get total tick count
 */
uint64_t get_tick_count() {
    return g_ticks;
}

/**
 * @brief Print scheduler statistics (for debugging)
 */
void print_scheduler_stats() {
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
             "[SCHEDULER] Ticks: %lu, Current: %s (PID %d)\n",
             g_ticks,
             g_current_process ? g_current_process->name : "none",
             g_current_process ? g_current_process->pid : -1);
    early_serial.write(buffer);

    // Count processes in ready queue
    int ready_count = 0;
    ProcessControlBlock* pcb = g_ready_queue_head;
    while (pcb) {
        ready_count++;
        pcb = pcb->next;
    }

    snprintf(buffer, sizeof(buffer),
             "[SCHEDULER] Ready queue: %d processes\n", ready_count);
    early_serial.write(buffer);
}

} // namespace xinim::kernel
