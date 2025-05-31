#include "../include/lib.hpp" // C++17 header

// Close the file descriptor 'fd'. Returns 0 on success or -1 on failure.
int close(int fd) {
    return callm1(FS, CLOSE, fd, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
}
