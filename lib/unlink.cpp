#include "../include/lib.hpp" // C++17 header

// Remove the directory entry named 'name'.
int unlink(const char *name) {
    return callm3(FS, UNLINK, 0, const_cast<char *>(name));
}
