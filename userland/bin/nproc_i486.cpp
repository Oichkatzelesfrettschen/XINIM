// nproc -- print number of processing units (GNU extension)
// Cleanroom C++23 implementation.
// XINIM runs on single-CPU i486: always reports 1.

#include <unistd.h>

int main() {
    write(1, "1\n", 2);
    return 0;
}
