#include "../include/stdio.hpp"

extern "C" int fflush(FILE *);

/* Flush all open stdio files */
// Flush all open stdio files at program exit.
void _cleanup(void) {
    for (int i = 0; i < NFILES; i++)
        if (_io_table[i] != nullptr)
            fflush(_io_table[i]);
}
