#include "../include/lib.hpp" // C++23 header

// Reposition the read/write file offset for descriptor 'fd'.
long lseek(int fd, long offset, int whence) {
    int k;
    M.m2_i1() = fd;
    M.m2_l1() = offset;
    M.m2_i2() = whence;
    k = callx(FS, LSEEK);
    if (k != OK)
        return ((long)k); /* send itself failed */
    return (M.m2_l1());
}
