/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stat.h"

int isatty(fd)
int fd;
{
  struct stat s;

  fstat(fd, &s);
  if ( (s.st_mode&S_IFMT) == S_IFCHR)
	return(1);
  else
	return(0);
}
