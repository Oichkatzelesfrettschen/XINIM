#include "../include/lib.hpp" // C++23 header

// Change ownership of 'name' to the given owner and group ids.
int chown(const char *name, int owner, int grp) {
    return callm1(FS, CHOWN, len(const_cast<char *>(name)), owner, grp, const_cast<char *>(name),
                  NIL_PTR, NIL_PTR);
}
