#include "../include/lib.hpp" // C++17 header

// Change the root directory for the calling process.
PUBLIC int chroot(const char *name) { return callm3(FS, CHROOT, 0, const_cast<char *>(name)); }
