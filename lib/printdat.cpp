/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stdio.hpp"

char  __stdin[BUFSIZ];
char  __stdout[BUFSIZ];

struct _io_buf _stdin = {
	0, 0, READMODE , __stdin, __stdin
};

struct _io_buf _stdout = {
	1, 0, WRITEMODE + PERPRINTF, __stdout, __stdout
};

struct _io_buf _stderr = {
	2, 0, WRITEMODE + UNBUFF, NULL, NULL
};

struct  _io_buf  *_io_table[NFILES] = {
	&_stdin,
	&_stdout,
	&_stderr,
	0
};
