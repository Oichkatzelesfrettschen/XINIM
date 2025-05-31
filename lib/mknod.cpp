#include "../include/lib.hpp" // C++17 header

// Create a special file with the given mode and device address.
int mknod(const char *name, int mode, int addr) {
    return callm1(FS, MKNOD, len(const_cast<char *>(name)), mode, addr,
                  const_cast<char *>(name), NIL_PTR, NIL_PTR);
}
