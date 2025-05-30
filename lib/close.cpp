#include "../include/lib.hpp" // C++17 header

// Close an open file descriptor.
PUBLIC int close(int fd) { return callm1(FS, CLOSE, fd, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }
