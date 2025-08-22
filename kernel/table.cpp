/**
 * @file table.cpp
 * @brief Defines the global task start table used by the kernel.
 *
 * This file contains the definitions for all shared global data in the system.
 * It uses a technique of redefining the `EXTERN` macro to an empty string,
 * so that when header files are included here, they allocate storage for
 * the shared variables rather than just declaring them as `extern`.
 *
 * The task table, `task`, is declared here with `constinit` to guarantee
 * compile-time initialization, providing a safe, modern alternative to
 * relying on linker behavior. The `tasks()` helper function returns a
 * `std::span` for type-safe, bounds-checked access to this critical global data.
 *
 * @ingroup kernel_init
 */

#include "../h/const.hpp"
#include "../h/type.hpp"
#include "const.hpp"
#include "type.hpp"
#undef EXTERN
#define EXTERN
#include "glo.hpp"
#include "proc.hpp"
#include <array>
#include <cstddef>
#include <span>

/// @brief Function pointer type for kernel tasks.
using TaskEntry = void (*)() noexcept;

/// @brief Entry point for the system task.
extern void sys_task() noexcept;
/// @brief Entry point for the clock task.
extern void clock_task() noexcept;
/// @brief Entry point for the floppy disk task.
extern void floppy_task() noexcept;
/// @brief Entry point for the winchester disk task.
extern void winchester_task() noexcept;
/// @brief Entry point for the terminal task.
extern void tty_task() noexcept;
/// @brief Entry point for the printer task.
extern void printer_task() noexcept;
/// @brief Entry point for the memory task.
extern void mem_task() noexcept;

/**
 * @brief Compile-time table of startup routines for system tasks.
 *
 * The order of entries must match the task identifiers defined in `com.hpp`.
 * This table is guaranteed to be initialized at compile time due to `constinit`.
 * Unused slots are explicitly initialised with `nullptr`.
 */
constinit std::array<TaskEntry, NR_TASKS + INIT_PROC_NR + 1> task = {
    printer_task, tty_task, winchester_task, floppy_task,
    mem_task,
    clock_task, sys_task,
    nullptr, ///< Was 0
    nullptr, ///< Was 0
    nullptr, ///< Was 0
    nullptr  ///< Was 0
};

/**
 * @brief Obtain a read-only span over the task table.
 *
 * This `constexpr` function provides a safe, C++23-idiomatic way to access the
 * global `task` table, ensuring bounds safety without exposing the underlying
 * array directly.
 *
 * @return A `std::span` providing read-only access to the task startup routines.
 */
[[nodiscard]] constexpr auto tasks() noexcept -> std::span<const TaskEntry> {
    return {task};
}