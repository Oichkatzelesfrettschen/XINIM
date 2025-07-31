/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stdio.h"

char *sprintf(buf,format,args)
char *buf, *format;
int args;
{
	FILE _tempfile;

	_tempfile._fd    = -1;
	_tempfile._flags = WRITEMODE + STRINGS;
	_tempfile._buf   = buf;
	_tempfile._ptr   = buf;

	_doprintf(&_tempfile,format,&args);
	putc('\0',&_tempfile);

	return buf;
}
