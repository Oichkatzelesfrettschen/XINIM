#include "../include/lib.hpp" // C++17 header

// Read up to 'nbytes' from file descriptor 'fd' into 'buffer'.
int read(int fd, char *buffer, int nbytes) {
    int n = callm1(FS, READ, fd, nbytes, 0, buffer, NIL_PTR, NIL_PTR);
    return n;
}
