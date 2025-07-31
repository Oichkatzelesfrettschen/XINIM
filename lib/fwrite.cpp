/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stdio.h"

fwrite(ptr, size, count, file)
unsigned size, count;
char *ptr;
FILE *file;
{
	unsigned s;
	unsigned ndone = 0;

	if (size)
		while ( ndone < count ) {
			s = size;
			do {
				putc(*ptr++, file);
				if (ferror(file))
					return(ndone);
			} 
			while (--s);
			ndone++;
		}
	return(ndone);
}
