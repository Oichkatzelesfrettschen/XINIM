#include "../include/lib.hpp" // C++23 header

// Open the file 'name' with the given mode and return a descriptor.
int open(const char *name, int mode) { return callm3(FS, OPEN, mode, const_cast<char *>(name)); }
