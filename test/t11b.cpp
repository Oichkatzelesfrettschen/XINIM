/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include <cstdio>
#include <cstdlib>

// Compare two strings and report if they differ.
static bool diff(const char *s1, const char *s2) {
    while (true) {
        if (*s1 == '\0' && *s2 == '\0')
            return false;
        if (*s1 != *s2)
            return true;
        ++s1;
        ++s2;
    }
}

// Display an error value for debugging.
static void e(int n) { std::printf("Error %d\n", n); }

// Validate that command line arguments were passed correctly.
int main(int argc, char *argv[]) {
    if (diff(argv[0], "t11b"))
        e(31);
    if (diff(argv[1], "abc"))
        e(32);
    if (diff(argv[2], "defghi"))
        e(33);
    if (diff(argv[3], "j"))
        e(34);
    if (argv[4] != nullptr)
        e(35);
    if (argc != 4)
        e(36);
    return 75;
}
