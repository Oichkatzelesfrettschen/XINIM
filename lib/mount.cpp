/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Mount the file system from 'special' onto 'name'.
int mount(const char *special, const char *name, int rwflag) {
    return callm1(FS, MOUNT, len(const_cast<char *>(special)),
                  len(const_cast<char *>(name)), rwflag,
                  const_cast<char *>(special), const_cast<char *>(name), NIL_PTR);
}
