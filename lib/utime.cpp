#include "../include/lib.hpp" // C++23 header

// Update the access and modification times of 'name'.
int utime(const char *name, long timp[2]) {
    M.m2_i1() = len(const_cast<char *>(name));
    M.m2_l1() = timp[0];
    M.m2_l2() = timp[1];
    M.m2_p1() = const_cast<char *>(name);
    return callx(FS, UTIME);
}
