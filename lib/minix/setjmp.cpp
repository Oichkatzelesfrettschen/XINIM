/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../../include/setjmp.hpp"
#include "../../include/lib.h"
#include <csetjmp>

/*
 * Alternate location for _setjmp/_longjmp used by certain Minix binaries.
 * Implementation identical to the generic versions.
 */
// Alternate location for _setjmp used by certain binaries.
int _setjmp(jmp_buf env) { return std::setjmp(env); }

// Alternate location for _longjmp used by certain binaries.
void _longjmp(jmp_buf env, int val) {
    if (val == 0) {
        val = 1;
    }
    std::longjmp(env, val);
}
