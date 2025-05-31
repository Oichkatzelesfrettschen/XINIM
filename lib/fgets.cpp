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

