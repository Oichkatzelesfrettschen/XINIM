/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

int main(int argc, char *argv[]) {
    char c;

    /* Echo argument, if present. */
    if (argc == 2) {
        std_err(argv[1]);
        std_err("\n");
    }
    close(0);
    open("/dev/tty0", 0);

    do {
        read(0, &c, 1);
    } while (c != '\n');
    exit(0);
}
