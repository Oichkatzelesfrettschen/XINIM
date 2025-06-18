#include "../include/lib.hpp" // C++23 header

// Change the current working directory to 'name'. Returns 0 on success or -1
// on failure with errno set.
int chdir(const char *name) { return callm3(FS, CHDIR, 0, const_cast<char *>(name)); }
