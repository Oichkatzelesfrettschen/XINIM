/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stdio.h"

FILE *freopen(name,mode,stream)
char *name,*mode;
FILE *stream;
{
	FILE *fopen();

	if ( fclose(stream) != 0 )
		return NULL;

	return fopen(name,mode);
}
