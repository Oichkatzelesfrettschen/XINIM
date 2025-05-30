#include "../include/lib.hpp" // C++17 header

// Create a pipe and return two file descriptors in 'fild'.
PUBLIC int pipe(int fild[2]) {
    int k;
    k = callm1(FS, PIPE, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    if (k >= 0) {
        fild[0] = M.m1_i1;
        fild[1] = M.m1_i2;
        return 0;
    }
    return k;
}
