/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/setjmp.hpp"
#include "../include/lib.hpp"
#include <stdlib.hpp>

/*
 * Portable implementation of _setjmp using the host C library.
 * The custom jmp_buf stores a pointer to the real jmp_buf.
 */
PUBLIC int _setjmp(jmp_buf env) {
    jmp_buf *real = safe_malloc(sizeof(jmp_buf));
    *(jmp_buf **)env = real;
    return setjmp(*real);
}

/*
 * Portable implementation of _longjmp using the host C library.
 */
PUBLIC void _longjmp(jmp_buf env, int val) {
    jmp_buf *real = *(jmp_buf **)env;
    if (val == 0) {
        val = 1;
    }
    longjmp(*real, val);
}
