#include "../include/stdio.hpp"
#include <unistd.h>

// External declaration for the flush function
extern "C" int __fflush(FILE *iop);

// Program cleanup - flushes all open stdio streams at exit
void _cleanup(void) {
    for (int i = 0; i < NFILES; ++i) {
        if (_io_table[i] != nullptr) {
            __fflush(_io_table[i]);
        }
    }

