#include "../include/lib.hpp" // C++17 header

// Open a file with the given mode.
PUBLIC int open(const char *name, int mode) {
    return callm3(FS, OPEN, mode, const_cast<char *>(name));
}
