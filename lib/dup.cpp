#include "../include/lib.hpp" // C++23 header

// Duplicate the given file descriptor. Returns the new descriptor on success
// or -1 on failure.
int dup(int fd) { return callm1(FS, DUP, fd, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }
