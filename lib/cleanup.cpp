#include "../include/stdio.h"

/* Flush all open stdio files */
void _cleanup(void)
{
	register int i;

	for ( i = 0 ; i < NFILES ; i++ )
		if ( _io_table[i] != NULL )
			fflush(_io_table[i]);
}
