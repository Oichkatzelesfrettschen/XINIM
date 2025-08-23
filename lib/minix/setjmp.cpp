#include "../../include/setjmp.hpp"
#include "../../include/lib.hpp"
#include <csetjmp>

/*
 * Alternate location for _setjmp/_longjmp used by certain Minix binaries.
 * Implementation identical to the generic versions.
 */
// Alternate location for _setjmp used by certain binaries.
int _setjmp(jmp_buf env) { return setjmp(env); }

// Alternate location for _longjmp used by certain binaries.
void _longjmp(jmp_buf env, int val) {
    if (val == 0) {
        val = 1;
    }
    longjmp(env, val);
}
