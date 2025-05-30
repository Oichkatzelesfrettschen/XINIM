#include "../../include/setjmp.h"
#include "../../include/lib.h"
#include <stdlib.h>

/*
 * Alternate location for _setjmp/_longjmp used by certain Minix binaries.
 * Implementation identical to the generic versions.
 */
PUBLIC int _setjmp(jmp_buf env) {
    jmp_buf *real = safe_malloc(sizeof(jmp_buf));
    *(jmp_buf **)env = real;
    return setjmp(*real);
}

PUBLIC void _longjmp(jmp_buf env, int val) {
    jmp_buf *real = *(jmp_buf **)env;
    if (val == 0) {
        val = 1;
    }
    longjmp(*real, val);
}
