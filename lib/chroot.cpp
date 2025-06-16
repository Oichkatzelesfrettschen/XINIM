#include "../include/lib.hpp" // C++23 header

// Change the root directory for the current process. Returns 0 on success or
// -1 on failure.
int chroot(const char *name) { return callm3(FS, CHROOT, 0, const_cast<char *>(name)); }
