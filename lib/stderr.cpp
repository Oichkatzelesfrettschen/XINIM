#include <unistd.h> // write system call

// Write a string to standard error.
void std_err(const char *s) {
    const char *p = s;
    while (*p != 0)
        p++;
    write(2, s, p - s);
}
