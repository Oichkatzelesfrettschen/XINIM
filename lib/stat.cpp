#include "../include/lib.hpp" // C++23 header

// Obtain information about the file named 'name'.
int stat(const char *name, char *buffer) {
    int n = callm1(FS, STAT, len(const_cast<char *>(name)), 0, 0, const_cast<char *>(name), buffer,
                   NIL_PTR);
    return n;
}
