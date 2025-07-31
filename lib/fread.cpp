/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stdio.h"

fread(ptr, size, count, file)
char *ptr;
unsigned size, count;
FILE *file;
{
	register int c;
	unsigned ndone = 0, s;

	ndone = 0;
	if (size)
		while ( ndone < count ) {
			s = size;
			do {
				if ((c = getc(file)) != EOF)
					*ptr++ = (char) c;
				else
					return(ndone);
			} while (--s);
			ndone++;
		}
	return(ndone);
}

