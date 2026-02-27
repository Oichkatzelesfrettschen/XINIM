/**
 * @file table.cpp
 * @brief Defines the global task start table and BIOS-related data.
 */

#include "sys/const.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "type.hpp"

// Define the variables here by redefining EXTERN
#undef EXTERN
#define EXTERN 
#include "glo.hpp"

#include <array>
#include <cstddef>
#include <span>

/// @brief Function pointer type for kernel tasks.
using TaskEntry = void (*)() noexcept;

extern "C" {
    void sys_task() noexcept;
    void clock_task() noexcept;
    void floppy_task() noexcept;
    void winchester_task() noexcept;
    void tty_task() noexcept;
    void printer_task() noexcept;
    void mem_task() noexcept;
    
    // Additional definitions not in glo.hpp
    unsigned int sizes[8] = { 0 };
    int vec_table[256] = { 0 };
    uint64_t get_base() noexcept { return 0; }
    int color = 1;
}

/**
 * @brief Compile-time table of startup routines for system tasks.
 */
constinit std::array<TaskEntry, NR_TASKS + INIT_PROC_NR + 1> task = {
    printer_task,
    tty_task,
    winchester_task,
    floppy_task,
    mem_task,
    clock_task,
    sys_task,
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr  
};

/**
 * @brief Obtain a read-only span over the task table.
 */
[[nodiscard]] constexpr auto tasks() noexcept -> std::span<const TaskEntry> {
    return {task};
}
