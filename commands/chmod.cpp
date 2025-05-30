/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/*
 * chmod [mode] files
 *  change mode of files
 *
 *  by Patrick van Kleef
 */

/* Program entry point */
int main(int argc, char *argv[]) {
    int i;
    int status = 0;
    int newmode;

    if (argc < 3) {
        Usage();
    }

    newmode = oatoi(argv[1]);

    for (i = 2; i < argc; i++) {
        if (access(argv[i], 0)) {
            prints("chmod: can't access %s\n", argv[i]);
            status++;
        } else if (chmod(argv[i], newmode) < 0) {
            prints("chmod: can't change %s\n", argv[i]);
            status++;
        }
    }
    exit(status);
}

/* Convert octal string to int */
static int oatoi(char *arg) {
    register c, i;

    i = 0;
    while ((c = *arg++) >= '0' && c <= '7')
        i = (i << 3) + (c - '0');
    if (c != '\0')
        Usage();
    return (i);
}

/* Print usage message */
static void Usage(void) {
    prints("Usage: chmod [mode] file ...\n");
    exit(255);
}
