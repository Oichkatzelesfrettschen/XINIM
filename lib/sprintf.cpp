/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/stdio.hpp"

char *sprintf(buf, format, args)
char *buf, *format;
int args;
{
    FILE _tempfile;

    _tempfile._fd = -1;
    _tempfile._flags = WRITEMODE + STRINGS;
    _tempfile._buf = buf;
    _tempfile._ptr = buf;

    _doprintf(&_tempfile, format, &args);
    putc('\0', &_tempfile);

    return buf;
}
