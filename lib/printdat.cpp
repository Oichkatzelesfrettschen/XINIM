/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

char __stdin[BUFSIZ];
char __stdout[BUFSIZ];

struct _io_buf _stdin = {0, 0, READMODE, __stdin, __stdin};

struct _io_buf _stdout = {1, 0, WRITEMODE + PERPRINTF, __stdout, __stdout};

struct _io_buf _stderr = {2, 0, WRITEMODE + UNBUFF, NULL, NULL};

struct _io_buf *_io_table[NFILES] = {&_stdin, &_stdout, &_stderr, 0};
