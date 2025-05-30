#include "../include/stdio.hpp"

fputs(s, file) register char *s;
FILE *file;
{
    while (*s)
        putc(*s++, file);
}
