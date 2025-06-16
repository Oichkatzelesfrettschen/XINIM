#pragma once

/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

// Legacy representation of a group database entry.
struct group {
    char *name;   // Name of the group
    char *passwd; // Encrypted group password
    int gid;      // Numerical group identifier
};
