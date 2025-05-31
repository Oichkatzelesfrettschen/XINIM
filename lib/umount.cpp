#include "../include/lib.hpp" // C++17 header

// Unmount the file system mounted at 'name'.
int umount(const char *name) {
    return callm3(FS, UMOUNT, 0, const_cast<char *>(name));
}
