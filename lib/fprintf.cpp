/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stdio.h"

fprintf (file, fmt, args)
FILE *file;
char *fmt;
int args;
{
	_doprintf (file, fmt, &args);
	if ( testflag(file,PERPRINTF) )
        	fflush(file);
}


printf (fmt, args)
char *fmt;
int args;
{
	_doprintf (stdout, fmt, &args);
	if ( testflag(stdout,PERPRINTF) )
        	fflush(stdout);
}
