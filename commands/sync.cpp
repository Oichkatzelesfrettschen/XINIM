/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* sync - flush the file system buffers.  Author: Andy Tanenbaum */

#include <unistd.h>

// Program entry point
int main() {
    // Flush all pending file system data to disk
    sync();
    return 0; // RAII not required since no resources are acquired
}
