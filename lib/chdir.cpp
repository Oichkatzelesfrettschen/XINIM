#include "../include/lib.hpp" // C++17 header

// Change the current working directory.
PUBLIC int chdir(const char *name) { return callm3(FS, CHDIR, 0, const_cast<char *>(name)); }
