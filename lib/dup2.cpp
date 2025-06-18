#include "../include/lib.hpp" // C++23 header

// Duplicate 'fd' to the descriptor 'fd2'. Returns the duplicated descriptor
// or -1 on failure.
int dup2(int fd, int fd2) { return callm1(FS, DUP, fd + 0100, fd2, 0, NIL_PTR, NIL_PTR, NIL_PTR); }
