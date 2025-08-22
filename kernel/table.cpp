/* The object file of "table.cpp" contains all the data.  In the *.h files,
 * declared variables appear with EXTERN in front of them, as in
 *
 *    EXTERN int x;
 *
 * Normally EXTERN is defined as extern, so when they are included in another
 * file, no storage is allocated.  If the EXTERN were not present, but just
 * say,
 *
 *    int x;
 *
 * then including this file in several source files would cause 'x' to be
 * declared several times.  While some linkers accept this, others do not,
 * so they are declared extern when included normally.  However, it must
 * be declared for real somewhere.  That is done here, but redefining
 * EXTERN as the null string, so the inclusion of all the *.h files in
 * table.cpp actually generates storage for them.  All the initialized
 * variables are also declared here, since
 *
 * extern int x = 4;
 *
 * is not allowed.  If such variables are shared, they must also be declared
 * in one of the *.h files without the initialization.
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

/// @brief Function pointer type for kernel tasks.
using TaskEntry = void (*)() noexcept;

/// @brief Entry point for the system task.
extern void sys_task() noexcept;
/// @brief Entry point for the clock task.
extern void clock_task() noexcept;
// extern void mem_task() noexcept; // mem_task not defined in provided files, assume similar
/// @brief Entry point for the floppy disk task.
extern void floppy_task() noexcept;
/// @brief Entry point for the winchester disk task.
extern void winchester_task() noexcept;
/// @brief Entry point for the terminal task.
extern void tty_task() noexcept;
/// @brief Entry point for the printer task.
extern void printer_task() noexcept;

/**
 * @brief Startup routine table for kernel tasks.
 *
 * The order of entries must match the task identifiers defined in
 * ../h/com.hpp.
 */
constexpr std::array<TaskEntry, NR_TASKS + INIT_PROC_NR + 1> task{
    printer_task, tty_task, winchester_task, floppy_task,
    nullptr, ///< Placeholder for mem_task if its signature changes / not available
    clock_task,   sys_task,
    nullptr, ///< Was 0
    nullptr, ///< Was 0
    nullptr, ///< Was 0
    nullptr  ///< Was 0
};
