#include "../include/lib.hpp" // C++23 header

// Create a new file with the specified mode. Returns a file descriptor on
// success or -1 on failure.
int creat(const char *name, int mode) { return callm3(FS, CREAT, mode, const_cast<char *>(name)); }
