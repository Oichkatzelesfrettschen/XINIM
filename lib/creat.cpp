#include "../include/lib.hpp" // C++17 header

// Create a new file with the specified mode.
PUBLIC int creat(char *name, int mode) { return callm3(FS, CREAT, mode, name); }
