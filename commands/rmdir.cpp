/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* rmdir - remove a directory		Author: Adri Koppes */

#include "../include/signal.hpp"
#include "../include/stat.hpp"

struct direct {
    unsigned short d_ino;
    char d_name[14];
};
int error = 0;

/* Program entry point */
int main(int argc, char **argv) {
    if (argc < 2) {
        prints("Usage: rmdir dir ...\n");
        exit(1);
    }
    signal(Signal::SIGHUP, SIG_IGN);
    signal(Signal::SIGINT, SIG_IGN);
    signal(Signal::SIGQUIT, SIG_IGN);
    signal(Signal::SIGTERM, SIG_IGN);
    while (--argc)
        remove(*++argv);
    if (error)
        exit(1);
}

/* Remove a directory */
static void remove(char *dirname) {
    struct direct d;
    struct stat s, cwd;
    register int fd = 0, sl = 0;
    char dots[128];

    if (stat(dirname, &s)) {
        stderr2(dirname, " doesn't exist\n");
        error++;
        return;
    }
    if ((s.st_mode & S_IFMT) != S_IFDIR) {
        stderr2(dirname, " not a directory\n");
        error++;
        return;
    }
    strcpy(dots, dirname);
    while (dirname[fd])
        if (dirname[fd++] == '/')
            sl = fd;
    dots[sl] = '\0';
    if (access(dots, 2)) {
        stderr2(dirname, " no permission\n");
        error++;
        return;
    }
    stat("", &cwd);
    if ((s.st_ino == cwd.st_ino) && (s.st_dev == cwd.st_dev)) {
        std_err("rmdir: can't remove current directory\n");
        error++;
        return;
    }
    if ((fd = open(dirname, 0)) < 0) {
        stderr2("can't read ", dirname);
        std_err("\n");
        error++;
        return;
    }
    while (read(fd, (char *)&d, sizeof(struct direct)) == sizeof(struct direct))
        if (d.d_ino != 0)
            if (strcmp(d.d_name, ".") && strcmp(d.d_name, "..")) {
                stderr2(dirname, " not empty\n");
                close(fd);
                error++;
                return;
            }
    close(fd);
    strcpy(dots, dirname);
    strcat(dots, "/.");
    unlink(dots);
    strcat(dots, ".");
    unlink(dots);
    if (unlink(dirname)) {
        stderr2("can't remove ", dirname);
        std_err("\n");
        error++;
        return;
    }
}

/* Print two error strings */
static void stderr2(const char *s1, const char *s2) {
    std_err("rmdir: ");
    std_err(s1);
    std_err(s2);
}
