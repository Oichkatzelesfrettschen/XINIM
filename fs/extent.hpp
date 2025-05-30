/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* Extent-based allocation structures. */
#ifndef FS_EXTENT_H
#define FS_EXTENT_H

#include "../h/type.hpp"

/* An extent describes a contiguous range of zones on disk. */
typedef struct {
    zone_nr e_start; /* first zone in the extent */
    zone_nr e_count; /* number of zones in the extent */
} extent;

#define NIL_EXTENT (extent *)0

#endif /* FS_EXTENT_H */
