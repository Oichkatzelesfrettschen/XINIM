#include "../include/lib.hpp" // C++17 header

extern char endbss; /* Symbol marking end of BSS provided by end.s */

/*
 * Return the base memory click.  The original assembly stub returned zero.
 */
PUBLIC phys_clicks get_base(void) { return 0; }

/*
 * Return the address of the end of the BSS segment.
 */
PUBLIC phys_clicks get_size(void) { return (phys_clicks)&endbss; }

/*
 * Return the total amount of memory available.
 */
PUBLIC phys_clicks get_tot_mem(void) { return 0; }
