#include <setjmp.h>
#include <stdlib.h>
#include "../include/setjmp.h"

/*
 * Portable implementation of _setjmp using the host C library.
 * The custom jmp_buf stores a pointer to the real jmp_buf.
 */
PUBLIC int _setjmp(jmp_buf env)
{
    jmp_buf *real = malloc(sizeof(jmp_buf));
    if (!real) {
        return -1;
    }
    *(jmp_buf **)env = real;
    return setjmp(*real);
}

/*
 * Portable implementation of _longjmp using the host C library.
 */
PUBLIC void _longjmp(jmp_buf env, int val)
{
    jmp_buf *real = *(jmp_buf **)env;
    if (val == 0) {
        val = 1;
    }
    longjmp(*real, val);
}
