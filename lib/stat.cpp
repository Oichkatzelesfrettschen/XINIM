#include "../include/lib.hpp" // C++17 header

// Retrieve file status into 'buffer'.
PUBLIC int stat(const char *name, char *buffer) {
    int n;
    n = callm1(FS, STAT, len(const_cast<char *>(name)), 0, 0, const_cast<char *>(name), buffer,
               NIL_PTR);
    return n;
}
