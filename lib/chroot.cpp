#include "../include/lib.hpp" // C++17 header

// Change the root directory for the current process.
int chroot(char *name) { return callm3(FS, CHROOT, 0, name); }
