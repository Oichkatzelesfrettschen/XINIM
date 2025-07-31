/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

struct tbuf {
    long b1, b2, b3, b4;
};
// Retrieve process times and store them into 'buf'.
int times(struct tbuf *buf) {
    int k = callm1(FS, TIMES, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
    buf->b1 = M.m4_l1();
    buf->b2 = M.m4_l2();
    buf->b3 = M.m4_l3();
    buf->b4 = M.m4_l4();
    return (k);
}
