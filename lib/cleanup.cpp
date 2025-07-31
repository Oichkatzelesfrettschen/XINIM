/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stdio.hpp"
#include <unistd.h>

// External declaration for the flush function
extern "C" int __fflush(FILE *iop);

// Program cleanup - flushes all open stdio streams at exit
void _cleanup(void) {
    for (int i = 0; i < NFILES; i++)
        if (_io_table[i] != nullptr)
            fflush(_io_table[i]);
}
