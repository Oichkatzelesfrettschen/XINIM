#include "../include/lib.hpp" // C++17 header

// Create a new link to the file named by 'name2' under the name 'name'.
int link(const char *name, const char *name2) {
    return callm1(FS, LINK, len(const_cast<char *>(name)),
                  len(const_cast<char *>(name2)), 0, const_cast<char *>(name),
                  const_cast<char *>(name2), NIL_PTR);
}
