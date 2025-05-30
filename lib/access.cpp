#include "../include/lib.hpp" // C++17 header

// Access a file to check permissions using C++ style parameters.
PUBLIC int access(const char *name, int mode) {
    return callm3(FS, ACCESS, mode, const_cast<char *>(name));
}
