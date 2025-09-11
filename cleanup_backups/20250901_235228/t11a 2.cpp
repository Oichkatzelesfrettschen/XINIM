#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

/**
 * @file t11a.cpp
 * @brief Checks argument passing, environment setup, and simple syscalls.
 */

/**
 * @brief Compare two C strings.
 * @return true if the strings differ, false otherwise.
 */
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

/**
 * @brief Print a formatted error message.
 */
static void e(int n) { std::printf("Error %d\n", n); }

/**
 * @brief Entry point verifying argument passing and file operations.
 *
 * Prints an error code on failure and returns 100 on success.
 */
int main(int argc, char *argv[], char *envp[]) {
    char aa[4];

    if (diff(argv[0], "t11a"))
        e(21);
    if (diff(argv[1], "arg0"))
        e(22);
    if (diff(argv[2], "arg1"))
        e(23);
    if (diff(argv[3], "arg2"))
        e(24);
    if (diff(envp[0], "spring"))
        e(25);
    if (diff(envp[1], "summer"))
        e(26);
    if (argc != 4)
        e(27);

    // Validate file contents.
    if (read(3, aa, 1000) != 2)
        e(28);
    if (aa[0] != 7 || aa[1] != 9)
        e(29);

    if (getuid() == 10)
        e(30);
    if (geteuid() != 10)
        e(31);
    if (getgid() == 20)
        e(32);
    if (getegid() != 20)
        e(33);

    if (open("t1", O_RDONLY) < 0)
        e(34);
    if (open("t2", O_RDONLY) < 0)
        e(35);
    return 100;
}
