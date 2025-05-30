#include "../../include/setjmp.hpp"
#include "../../include/lib.h"
#include <csetjmp>

/*
 * Alternate location for _setjmp/_longjmp used by certain Minix binaries.
 * Implementation identical to the generic versions.
 */
PUBLIC int _setjmp(jmp_buf env) { return std::setjmp(env); }

PUBLIC void _longjmp(jmp_buf env, int val) {
    if (val == 0) {
        val = 1;
    }
    std::longjmp(env, val);
}
