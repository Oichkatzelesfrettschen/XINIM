// Modernized for C++23

/* rm - remove files			Author: Adri Koppes */

#include "stat.hpp"

struct direct {
    unsigned short d_ino;
    char d_name[14];
};

int errors = 0;
int fflag = 0;
int iflag = 0;
int rflag = 0;
int exstatus;

/* Program entry point */
int main(int argc, char *argv[]) {
    char *opt;

    if (argc < 2)
        usage();
    *++argv;
    --argc;
    while (**argv == '-') {
        opt = *argv;
        while (*++opt != '\0')
            switch (*opt) {
            case 'f':
                fflag++;
                break;
            case 'i':
                iflag++;
                break;
            case 'r':
                rflag++;
                break;
            default:
                std_err("rm: unknown option\n");
                usage();
                break;
            }
        argc--;
        *++argv;
    }
    if (argc < 1)
        usage();
    while (argc--)
        remove(*argv++);
    exstatus = (errors == 0 ? 0 : 1);
    if (fflag)
        exstatus = 0;
    exit(exstatus);
}

/* Print usage information */
static void usage(void) {
    std_err("Usage: rm [-fir] file\n");
    exit(1);
}

/* Remove a file or directory */
static void remove(char *name) {
    struct stat s;
    struct direct d;
    char rname[128], *strcpy(), *strcat();
    int fd;

    if (stat(name, &s)) {
        if (!fflag)
            stderr3("rm: ", name, " non-existent\n");
        errors++;
        return;
    }
    if (iflag) {
        stderr3("rm: remove ", name, "? ");
        if (!confirm())
            return;
    }
    if ((s.st_mode & S_IFMT) == S_IFDIR) {
        if (rflag) {
            if ((fd = open(name, 0)) < 0) {
                if (!fflag)
                    stderr3("rm: can't open ", name, "\n");
                errors++;
                return;
            }
            while (read(fd, (char *)&d, sizeof(struct direct)) > 0) {
                if (d.d_ino && strcmp("..", d.d_name) && strcmp(".", d.d_name)) {
                    strcpy(rname, name);
                    strcat(rname, "/");
                    strcat(rname, d.d_name);
                    remove(rname);
                }
            }
            close(fd);
            rem_dir(name);
        } else {
            if (!fflag)
                stderr3("rm: ", name, " is a directory\n");
            errors++;
            return;
        }
    } else {
        if (access(name, 2) && !fflag) {
            stderr3("rm: remove ", name, " with mode ");
            octal(s.st_mode & 0777);
            std_err("? ");
            if (!confirm())
                return;
        }
        if (unlink(name)) {
            if (!fflag)
                stderr3("rm: ", name, " not removed\n");
            errors++;
        }
    }
}

/* Remove a directory using rmdir */
static void rem_dir(char *name) {
    int status;

    switch (fork()) {
    case -1:
        std_err("rm: can't fork\n");
        errors++;
        return;
    case 0:
        execl("/bin/rmdir", "rmdir", name, 0);
        execl("/usr/bin/rmdir", "rmdir", name, 0);
        std_err("rm: can't exec rmdir\n");
        exit(1);
    default:
        wait(&status);
        errors += status;
    }
}

/* Ask user for confirmation */
static int confirm(void) {
    char c, t;
    read(0, &c, 1);
    t = c;
    do
        read(0, &t, 1);
    while (t != '\n' && t != -1);
    return (c == 'y' || c == 'Y');
}

/* Print NUM in octal */
static void octal(unsigned int num) {
    char a[4];

    a[0] = (((num >> 6) & 7) + '0');
    a[1] = (((num >> 3) & 7) + '0');
    a[2] = ((num & 7) + '0');
    a[3] = 0;
    std_err(a);
}

/* Print three error strings */
static void stderr3(const char *s1, const char *s2, const char *s3) {
    std_err(s1);
    std_err(s2);
    std_err(s3);
}
