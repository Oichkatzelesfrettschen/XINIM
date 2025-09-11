/**
 * @file abort.h
 * @brief Cross-language wrapper for program termination.
 */
#ifndef XINIM_ABORT_H
#define XINIM_ABORT_H

#ifdef __cplusplus
#include <cstdlib>
/**
 * @brief Terminate the process without invoking signal handlers.
 *
 * The function exits the application with status code 99 and never
 * returns. It is provided both as a C linkage declaration and an
 * inline C++ helper.
 */
extern "C" _Noreturn void xinim_abort(void) noexcept;
inline _Noreturn void xinim_abort(void) noexcept { std::exit(99); }
#else
_Noreturn void xinim_abort(void);
#endif

#endif // XINIM_ABORT_H
