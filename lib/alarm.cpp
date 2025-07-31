/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Request an alarm after the specified number of seconds.
// Returns the previous alarm value.
int alarm(unsigned int sec) {
    return callm1(MM, ALARM, static_cast<int>(sec), 0, 0, NIL_PTR, NIL_PTR,
                  NIL_PTR);
}
