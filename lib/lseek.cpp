#include "../include/lib.hpp" // C++17 header

// Reposition read/write file offset.
PUBLIC long lseek(int fd, long offset, int whence) {
    int k;
    M.m2_i1 = fd;
    M.m2_l1 = offset;
    M.m2_i2 = whence;
    k = callx(FS, LSEEK);
    if (k != OK)
        return static_cast<long>(k); /* send itself failed */
    return M.m2_l1;
}
