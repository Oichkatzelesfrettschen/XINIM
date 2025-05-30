#include "../include/lib.hpp" // C++17 header

/*
 * Entry point for signal trampolines.
 * Merely returns so the caller can resume execution.
 */
PUBLIC int begsig() { return 0; }
