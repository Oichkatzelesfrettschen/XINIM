/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* Extent-based allocation structures. */
#ifndef FS_EXTENT_H
#define FS_EXTENT_H

#include "../h/type.hpp"

/* An extent describes a contiguous range of zones on disk. */
struct extent {
    zone_nr e_start; /* first zone in the extent */
    zone_nr e_count; /* number of zones in the extent */
};

#define NIL_EXTENT (extent *)0

#endif /* FS_EXTENT_H */
