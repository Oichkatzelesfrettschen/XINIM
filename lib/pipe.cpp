#include "../include/lib.hpp" // C++23 header

// Create an anonymous pipe and store the file descriptors into 'fild'.
int pipe(int fild[2]) {
    int k = callm1(FS, PIPE, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    if (k >= 0) {
        fild[0] = M.m1_i1();
        fild[1] = M.m1_i2();
        return (0);
    } else
        return (k);
}
