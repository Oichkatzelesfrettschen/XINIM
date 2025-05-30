/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

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
