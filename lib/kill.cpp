/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Send signal 'sig' to process number 'proc'.
int kill(int proc, int sig) {
    return callm1(MM, KILL, proc, sig, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}
