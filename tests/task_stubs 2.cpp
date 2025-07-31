#include "../kernel/proc.hpp"

/**
 * \file
 * \brief Stub implementations of kernel tasks used for unit tests.
 *
 * Each function defined here mimics a real kernel service task. Tests link
 * against these minimal stubs instead of the full implementations so that the
 * real tasks are not required during unit testing.
 */

/**
 * \brief Stub for the printer task.
 *
 * Simulates the kernel component that manages printer operations. The function
 * body is intentionally empty because unit tests only require the symbol for
 * linking.
 */
void printer_task() noexcept {}

/**
 * \brief Stub for the TTY task.
 *
 * Represents the terminal handling service and exists solely to satisfy linker
 * dependencies.
 */
void tty_task() noexcept {}

/**
 * \brief Stub for the Winchester disk task.
 *
 * Acts as a placeholder for the driver that would normally control Winchester
 * disks during system execution.
 */
void winchester_task() noexcept {}

/**
 * \brief Stub for the floppy disk task.
 *
 * Provides a minimal replacement for the floppy disk service required in a
 * real system.
 */
void floppy_task() noexcept {}

/**
 * \brief Stub for the clock task.
 *
 * Supplies a minimal clock service used when tests interact with time-related
 * kernel functionality.
 */
void clock_task() noexcept {}

/**
 * \brief Stub for the system task.
 *
 * Serves as the stand-in for the primary system management task referenced by
 * various parts of the kernel.
 */
void sys_task() noexcept {}
