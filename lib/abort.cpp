/**
 * @file abort.cpp
 * @brief Minimal stub for the C @c abort function.
 */

#include <cstdlib>

namespace xinim {

/**
 * @brief Terminate the program abnormally.
 *
 * The implementation simply exits with status code 99 rather than
 * raising @c SIGABRT. This mirrors the behaviour of the historical
 * MINIX implementation where generating a core dump was not required.
 */
[[noreturn]] void abort() { std::exit(99); }

} // namespace xinim
