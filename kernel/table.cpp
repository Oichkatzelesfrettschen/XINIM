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
#include <cstddef> // For nullptr

// Function declarations for tasks. Assuming they are now void () noexcept.
extern void sys_task() noexcept;
extern void clock_task() noexcept;
// extern void mem_task() noexcept; // mem_task not defined in provided files, assume similar
extern void floppy_task() noexcept;
extern void winchester_task() noexcept;
extern void tty_task() noexcept;
extern void printer_task() noexcept;

/* The startup routine of each task is given below, from -NR_TASKS upwards.
 * The order of the names here MUST agree with the numerical values assigned to
 * the tasks in ../h/com.hpp.
 */
/**
 * @brief Kernel task start table.
 *
 * @pre Task entry points such as ::sys_task and ::clock_task are valid.
 * @post Scheduler uses this table during boot to spawn tasks.
 * @warning Placeholder @c nullptr entries remain for unimplemented tasks.
 */
// Changed array type from int(*)() to void(*)() noexcept
void (*task[NR_TASKS + INIT_PROC_NR + 1])() noexcept = {
    printer_task,
    tty_task,
    winchester_task,
    floppy_task,
    nullptr, // Placeholder for mem_task if its signature changes / not available
    clock_task,
    sys_task,
    nullptr, // Was 0
    nullptr, // Was 0
    nullptr, // Was 0
    nullptr  // Was 0
};
