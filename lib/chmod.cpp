#include "../include/lib.hpp" // C++17 header

// Change the permissions of 'name'.
PUBLIC int chmod(const char *name, int mode) {
    return callm3(FS, CHMOD, mode, const_cast<char *>(name));
}
