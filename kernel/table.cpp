/**
 * @file table.cpp
 * @brief Defines the global task start table used by the kernel.
 *
 * The object file of `table.cpp` contains all global data. In the header
 * files, variables appear with the macro #EXTERN to avoid allocating storage
 * multiple times. By redefining #EXTERN to the empty string here, including
 * the headers provides the actual definitions. Any shared initialized
 * variables must also be declared in a header without the initialization.
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

/// @brief Entry point for the system task.
extern void sys_task() noexcept;
/// @brief Entry point for the clock task.
extern void clock_task() noexcept;
// extern void mem_task() noexcept; // mem_task not defined in provided files, assume similar
/// @brief Entry point for the floppy disk task.
extern void floppy_task() noexcept;
/// @brief Entry point for the winchester disk task.
extern void winchester_task() noexcept;
/// @brief Entry point for the TTY task.
extern void tty_task() noexcept;
/// @brief Entry point for the printer task.
extern void printer_task() noexcept;

/**
 * @brief Compile-time table of startup routines for system tasks.
 *
 * The order of entries must match the task identifiers defined in
 * `com.hpp`. Unused slots are explicitly initialised with `nullptr`.
 */
constinit std::array<void (*)() noexcept, NR_TASKS + INIT_PROC_NR + 1> task{
    printer_task, tty_task, winchester_task, floppy_task, nullptr, clock_task,
    sys_task,     nullptr,  nullptr,         nullptr,     nullptr};

/**
 * @brief Obtain a read-only span over the task table.
 * @return span providing access to the task startup routines.
 */
[[nodiscard]] constexpr auto tasks() noexcept -> std::span<void (*)() noexcept> { return {task}; }
