/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

// Wrapper around the ACCESS system call using the new interface.
//
// Parameters:
//   name - path to test
//   mode - access mode bits
//
// Returns 0 on success or -1 with errno set on failure.
int access(const char *name, int mode) {
    // Delegate to the file system server via callm3.
    return callm3(FS, ACCESS, mode, const_cast<char *>(name));
}
