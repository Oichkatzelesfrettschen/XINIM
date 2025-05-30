#include "../include/lib.hpp" // C++17 header

// Change the current working directory to 'name'.
PUBLIC int chdir(char *name) { return callm3(FS, CHDIR, 0, name); }
