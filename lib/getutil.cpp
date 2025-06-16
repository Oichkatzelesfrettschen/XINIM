#include "../include/lib.hpp" // C++23 header

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
