#include "../include/lib.hpp" // C++23 header

// Wait for a child process to change state.
int wait(int *status) {
    int k = callm1(MM, WAIT, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    *status = M.m2_i1();
    return k;
}
