/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* sleep - suspend a process for x sec		Author: Andy Tanenbaum */

main(argc, argv) int argc;
char *argv[];
{
    register seconds;
    register char c;

    seconds = 0;

    if (argc != 2) {
        std_err("Usage: sleep time\n");
        exit(1);
    }

    while (c = *(argv[1])++) {
        if (c < '0' || c > '9') {
            std_err("sleep: bad arg\n");
            exit(1);
        }
        seconds = 10 * seconds + (c - '0');
    }

    /* Now sleep. */
    sleep(seconds);
    exit(0);
}
