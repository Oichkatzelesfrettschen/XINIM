#include "../include/lib.hpp" // C++23 header

// Change the permissions of the file named 'name'. Returns 0 on success or
// -1 on failure.
int chmod(const char *name, int mode) { return callm3(FS, CHMOD, mode, const_cast<char *>(name)); }
