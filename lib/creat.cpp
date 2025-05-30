#include "../include/lib.hpp" // C++17 header

// Create a new file with permissions 'mode'.
PUBLIC int creat(const char *name, int mode) {
    return callm3(FS, CREAT, mode, const_cast<char *>(name));
}
