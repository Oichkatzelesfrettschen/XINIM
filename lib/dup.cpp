#include "../include/lib.hpp" // C++17 header

// Duplicate the given file descriptor.
PUBLIC int dup(int fd) { return callm1(FS, DUP, fd, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }
