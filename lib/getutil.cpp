/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/lib.hpp"

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
