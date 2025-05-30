/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

/* su - become super-user		Author: Patrick van Kleef */

#include "pwd.hpp"
#include "sgtty.hpp"
#include "stdio.hpp"

/* Program entry point */
int main(int argc, char *argv[]) {
    register char *name;
    char *crypt();
    char *shell = "/bin/sh";
    int nr;
    char password[14];
    struct sgttyb args;
    register struct passwd *pwd;
    struct passwd *getpwnam();

    if (argc > 1)
        name = argv[1];
    else
        name = "root";

    if ((pwd = getpwnam(name)) == 0) {
        std_err("Unknown id: ");
        std_err(name);
        std_err("\n");
        exit(1);
    }

    if (pwd->pw_passwd[0] != '\0' && getuid() != 0) {
        std_err("Password: ");
        ioctl(0, TIOCGETP, &args); /* get parameters */
        args.sg_flags = args.sg_flags & (~ECHO);
        ioctl(0, TIOCSETP, &args);
        nr = read(0, password, 14);
        password[nr - 1] = 0;
        putc('\n', stderr);
        args.sg_flags = args.sg_flags | ECHO;
        ioctl(0, TIOCSETP, &args);
        if (strcmp(pwd->pw_passwd, crypt(password, pwd->pw_passwd))) {
            std_err("Sorry\n");
            exit(2);
        }
    }
    setgid(pwd->pw_gid);
    setuid(pwd->pw_uid);
    if (pwd->pw_shell[0])
        shell = pwd->pw_shell;
    execn(shell);
    std_err("No shell\n");
    exit(3);
}
