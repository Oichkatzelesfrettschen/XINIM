/**
 * @file abort.c
 * @brief Minimal, idiomatic C23 abort routine for XINIM and related utilities.
 *
 * This routine is intended as a safe, non-core-dumping "abort" for OS, kernel,
 * and educational userspace code. It always exits the process with status 99.
 * It is not signal-based and does not invoke handlers, for maximal portability.
 */

#include <stdlib.h>

/**
 * @brief Abort the program, exiting with status 99 (no core dump).
 *
 * This is a C23, strictly-standards-compliant alternative to the
 * traditional abort(3) and is used as the canonical fatal-termination
 * endpoint for the XINIM kernel, tests, and userland.
 */
extern "C" {
[[noreturn]] void xinim_abort(void) {
    _Exit(99); // Fast, async-signal-safe, no flushing or unwinding.
}
}
