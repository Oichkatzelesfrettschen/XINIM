/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* ln - link a file		Author: Andy Tanenbaum */

#include "stat.hpp"
char name[17];
struct stat stb;

int main(int argc, char **argv) {
    char *file1, *file2;

    if (argc < 2 || argc > 3)
        usage();
    if (access(argv[1], 0) < 0) {
        std_err("ln: cannot access ");
        std_err(argv[1]);
        std_err("\n");
        exit(1);
    }
    if (stat(argv[1], &stb) >= 0 && (stb.st_mode & S_IFMT) == S_IFDIR)
        usage();
    file1 = argv[1];

    /* "ln file" means "ln file ." */
    if (argc == 2)
        file2 = ".";
    else
        file2 = argv[2];

    /* Check to see if target is a directory. */
    if (stat(file2, &stb) >= 0 && (stb.st_mode & S_IFMT) == S_IFDIR) {
        strcpy(name, file2);
        strcat(name, "/");
        strcat(name, last_comp(file1));
        file2 = name;
    }

    if (link(file1, file2)) {
        std_err("ln: Can't link\n");
        exit(1);
    }
    exit(0);
}

static char *last_comp(char *s) {
    /* Return pointer to last component of string. */
    int n;
    n = strlen(s);
    while (n--)
        if (*(s + n) == '/')
            return (s + n + 1);
    return (s);
}

static void usage() {
    std_err("Usage: ln file1 [file2]\n");
    exit(1);
}
