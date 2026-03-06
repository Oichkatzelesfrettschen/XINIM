#pragma once
/**
 * @file process_lifecycle.hpp
 * @brief Process exit and wait (reaping) for XINIM microkernel.
 *
 * WHY: Without process exit/cleanup, every exit halts the CPU and leaks
 *      resources. Without wait, parent processes cannot reap children.
 *
 * WHAT: process_exit() marks PCB as ZOMBIE, dequeues from scheduler,
 *       stores exit status, notifies waiting parent. process_wait()
 *       scans children for ZOMBIEs, blocks parent if none available.
 */

#include "../include/sys/type.hpp"
#include <cstdint>

namespace xinim::kernel {

/**
 * @brief Exit a process: mark as ZOMBIE, dequeue, store status.
 *
 * Steps:
 * 1. Set PCB state to ZOMBIE
 * 2. Store exit_status
 * 3. Dequeue from scheduler
 * 4. Free user stack via heap_free()
 * 5. Notify parent if it is waiting (WAIT_CHILD blocked)
 *
 * @param pid     Process ID to exit
 * @param status  Exit status code
 */
void process_exit(xinim::pid_t pid, int status);

/**
 * @brief Wait for a child process to exit.
 *
 * If child_pid > 0: wait for specific child.
 * If child_pid == -1: wait for any child.
 *
 * If a matching ZOMBIE is found: copies exit status, frees PCB, returns child PID.
 * If no ZOMBIE: blocks parent with WAIT_CHILD.
 *
 * @param parent_pid  Waiting parent's PID
 * @param child_pid   Which child to wait for (-1 = any)
 * @param status_out  Where to store the child's exit status
 * @return            Child PID on success, -1 if no children
 */
xinim::pid_t process_wait(xinim::pid_t parent_pid, xinim::pid_t child_pid,
                          int* status_out);

} // namespace xinim::kernel
