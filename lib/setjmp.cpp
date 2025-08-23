#include "../include/setjmp.hpp"
#include "../include/lib.hpp" // C++23 header
#include <csetjmp>

/*
 * Portable implementation of _setjmp using the host C library.
 * The custom jmp_buf stores a pointer to the real jmp_buf.
 */
// Portable implementation of _setjmp using the host C library.
int _setjmp(jmp_buf env) { return setjmp(env); }

/*
 * Portable implementation of _longjmp using the host C library.
 */
// Portable implementation of _longjmp using the host C library.
void _longjmp(jmp_buf env, int val) {
    if (val == 0) {
        val = 1;
    }
    longjmp(env, val);
}
