#include "../include/lib.h"

/*
 * Entry point for signal trampolines.
 * Merely returns so the caller can resume execution.
 */
PUBLIC int begsig()
{
    return 0;
}
