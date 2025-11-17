/**
 * @file scheduler.hpp
 * @brief Preemptive scheduler interface
 *
 * @ingroup kernel
 */

#ifndef XINIM_KERNEL_SCHEDULER_HPP
#define XINIM_KERNEL_SCHEDULER_HPP

#include <cstdint>
#include "../include/sys/type.hpp"

namespace xinim::kernel {

// Forward declarations
struct ProcessControlBlock;
enum class BlockReason;

/**
 * @brief Initialize scheduler subsystem
 *
 * Called during kernel initialization.
 */
void initialize_scheduler();

/**
 * @brief Start scheduler and begin preemptive multitasking
 *
 * This function never returns. It loads the first process and
 * enables timer interrupts.
 */
[[noreturn]] void start_scheduler();

/**
 * @brief Main scheduling function (called from timer interrupt)
 *
 * Picks next process and performs context switch.
 */
void schedule();

/**
 * @brief Add process to scheduler
 *
 * @param pcb Process control block to add
 */
void scheduler_add_process(ProcessControlBlock* pcb);

/**
 * @brief Get currently running process
 *
 * @return Pointer to current PCB, or nullptr if none
 */
ProcessControlBlock* get_current_process();

/**
 * @brief Block current process
 *
 * @param reason Why the process is blocking
 * @param wait_source PID to wait for (if IPC_RECV), or 0
 */
void block_current_process(BlockReason reason, xinim::pid_t wait_source = 0);

/**
 * @brief Unblock a process
 *
 * @param pcb Process to unblock
 */
void unblock_process(ProcessControlBlock* pcb);

/**
 * @brief Find process by PID
 *
 * @param pid Process ID to find
 * @return Pointer to PCB, or nullptr if not found
 */
ProcessControlBlock* find_process_by_pid(xinim::pid_t pid);

/**
 * @brief Get total tick count
 *
 * @return Number of timer ticks since boot
 */
uint64_t get_tick_count();

/**
 * @brief Print scheduler statistics (for debugging)
 */
void print_scheduler_stats();

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_SCHEDULER_HPP */
