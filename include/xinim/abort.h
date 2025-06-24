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
extern "C" [[noreturn]] void xinim_abort() noexcept;
inline void xinim_abort() noexcept { std::exit(99); }
#else
[[noreturn]] void xinim_abort(void) noexcept;
#endif

#endif // XINIM_ABORT_H
