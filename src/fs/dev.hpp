#pragma once
// Modernized for C++23

#include "sys/type.hpp"

/* Device table.  This table is indexed by major device number.  It provides
 * the link between major device numbers and the routines that process them.
 */

EXTERN struct dmap {
    int (*dmap_open)(int, message *);
    int (*dmap_rw)(int, message *);
    int (*dmap_close)(int, message *);
    int dmap_task;
} dmap[];
