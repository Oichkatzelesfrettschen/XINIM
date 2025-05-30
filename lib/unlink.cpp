#include "../include/lib.hpp" // C++17 header

// Remove a directory entry.
PUBLIC int unlink(const char *name) { return callm3(FS, UNLINK, 0, const_cast<char *>(name)); }
