#include <cstdio>
#include <cstdlib>

// Compare two strings. Return true if they differ.
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

// Report an error value.
static void e(int n) { std::printf("Error %d\n", n); }

// Ensure argument parsing functions properly.
int main(int argc, char *argv[]) {
    if (diff(argv[0], "t4b"))
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
