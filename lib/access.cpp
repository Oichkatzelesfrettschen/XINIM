#include "../include/lib.hpp" // C++17 header

// Wrapper around the ACCESS system call using the new interface.
int access(char *name, int mode) {
    // Delegate to the file system server via callm3.
    return callm3(FS, ACCESS, mode, name);
}
