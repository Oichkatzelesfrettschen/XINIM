/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/setjmp.hpp"
#include "../include/lib.hpp" // C++17 header
#include <csetjmp>

/*
 * Portable implementation of _setjmp using the host C library.
 * The custom jmp_buf stores a pointer to the real jmp_buf.
 */
// Portable implementation of _setjmp using the host C library.
int _setjmp(jmp_buf env) {
    /* Simply forward to the standard facility. */
    return std::setjmp(env);
}

/*
 * Portable implementation of _longjmp using the host C library.
 */
// Portable implementation of _longjmp using the host C library.
void _longjmp(jmp_buf env, int val) {
    if (val == 0) {
        val = 1;
    }
    std::longjmp(env, val);
}
