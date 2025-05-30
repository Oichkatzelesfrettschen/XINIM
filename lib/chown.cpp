#include "../include/lib.hpp" // C++17 header

// Change ownership of 'name' to the given owner and group ids.
PUBLIC int chown(char *name, int owner, int grp) {
    return callm1(FS, CHOWN, len(name), owner, grp, name, NIL_PTR, NIL_PTR);
}
