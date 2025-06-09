/**
 * @file abort.cpp
 * @brief Minimal stub for the C @c abort function.
 */

#include <stdlib.h>

/**
 * @brief Terminate the program abnormally.
 *
 * Simply exits with status code 99 to indicate failure. In a full
 * implementation this would raise SIGABRT and generate a core dump.
 */
[[noreturn]] void abort() { exit(99); }
