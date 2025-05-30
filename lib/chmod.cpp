#include "../include/lib.hpp" // C++17 header

// Change the permissions of the file named 'name'.
PUBLIC int chmod(char *name, int mode) { return callm3(FS, CHMOD, mode, name); }
