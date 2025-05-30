#include "../include/lib.hpp" // C++17 header

// Close the file descriptor 'fd'.
PUBLIC int close(int fd) { return callm1(FS, CLOSE, fd, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR); }
