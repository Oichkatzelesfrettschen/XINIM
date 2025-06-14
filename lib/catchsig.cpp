#include "../include/lib.hpp" // C++17 header

/**
 * @brief Entry point for signal trampolines.
 *
 * This function merely returns so the caller can resume execution.
 *
 * @return Always zero.
 */
int begsig() noexcept { return 0; }
