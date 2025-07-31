/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header

extern char endbss; /* Symbol marking end of BSS provided by end.s */

/*
 * Return the base memory click.  The original assembly stub returned zero.
 */
// Return the base memory click. The original assembly stub returned zero.
phys_clicks get_base() { return 0; }

/*
 * Return the address of the end of the BSS segment.
 */
// Return the address of the end of the BSS segment.
phys_clicks get_size() { return reinterpret_cast<phys_clicks>(&endbss); }

/*
 * Return the total amount of memory available.
 */
// Return the total amount of memory available.
phys_clicks get_tot_mem() { return 0; }
