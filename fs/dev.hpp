#pragma once
// Modernized for C++17

/* Device table.  This table is indexed by major device number.  It provides
 * the link between major device numbers and the routines that process them.
 */

struct dmap {
    int (*dmap_open)();
    int (*dmap_rw)();
    int (*dmap_close)();
    int dmap_task;
};

extern struct dmap dmap[];
