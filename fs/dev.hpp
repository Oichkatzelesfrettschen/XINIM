/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* Device table.  This table is indexed by major device number.  It provides
 * the link between major device numbers and the routines that process them.
 */

EXTERN struct dmap {
    int (*dmap_open)();
    int (*dmap_rw)();
    int (*dmap_close)();
    int dmap_task;
} dmap[];
