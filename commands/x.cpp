/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

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
