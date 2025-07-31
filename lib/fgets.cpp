/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/stdio.hpp"


char *fgets(char *str, unsigned n, FILE *file) {
    int ch;
    char *ptr = str;
    while (--n > 0 && (ch = getc(file)) != EOF) {
        *ptr++ = ch;
        if (ch == '\n')
            break;
    }
    if (ch == EOF && ptr == str)
        return nullptr;
    *ptr = '\0';
    return str;
}

