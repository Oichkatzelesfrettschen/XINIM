#include "../include/lib.hpp" // C++23 header

/**
 * @brief Entry point for signal trampolines.
 *
 * This function merely returns so the caller can resume execution.
 *
 * @return Always zero.
 * @sideeffects None.
 * @thread_safety Safe for concurrent invocation.
 */
int begsig() noexcept { return 0; }
