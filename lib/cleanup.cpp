#include "../include/stdio.hpp"

// External declaration for the minimal stdio flush routine.
extern "C" int mnx_fflush(FILE *);

// Flush all open stdio files at program exit.
void _cleanup() {
    // Iterate over each potential stream in the table.
    for (int i = 0; i < NFILES; ++i) {
        if (_io_table[i] != nullptr) {
            // Ensure buffered output is written before exit.
            mnx_fflush(_io_table[i]);
        }
    }
}
