/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

char buf[30000];
main() {
    int i, n;

    while (1) {
        n = read(0, buf, 30000);
        for (i = 0; i < n; i++)
            if (buf[i] == 015) {
                printf("DOS\n");
                exit();
            }
        if (n < 30000)
            exit(0);
    }
}
