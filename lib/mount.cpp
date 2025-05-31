#include "../include/lib.hpp" // C++17 header

// Mount the file system from 'special' onto 'name'.
int mount(const char *special, const char *name, int rwflag) {
    return callm1(FS, MOUNT, len(const_cast<char *>(special)),
                  len(const_cast<char *>(name)), rwflag,
                  const_cast<char *>(special), const_cast<char *>(name), NIL_PTR);
}
