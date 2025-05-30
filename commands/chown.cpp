/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/*
 * chown username file ...
 *
 * By Patrick van Kleef
 *
 */

#include "../h/type.hpp"
#include "pwd.hpp"
#include "stat.hpp"
#include "stdio.hpp"

/* Program entry point */
int main(int argc, char *argv[]) {
    int i, status = 0;
    struct passwd *pwd, *getpwnam();
    struct stat stbuf;

    if (argc < 3) {
        fprintf(stderr, "Usage: chown uid file ...\n");
        exit(1);
    }

    if ((pwd = getpwnam(argv[1])) == 0) {
        fprintf(stderr, "Unknown user id: %s\n", argv[1]);
        exit(4);
    }

    for (i = 2; i < argc; i++) {
        if (stat(argv[i], &stbuf) < 0) {
            perror(argv[i]);
            status++;
        } else if (chown(argv[i], pwd->pw_uid, stbuf.st_gid) < 0) {
            fprintf(stderr, "%s: not changed\n", argv[i]);
            status++;
        }
    }
    exit(status);
}
