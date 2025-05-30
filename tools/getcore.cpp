/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

main() {
    int fd, fd1, n;
    char buf[512], buf1[1536];

    fd = open("/dev/fd0", 0);
    read(fd, buf1, 3 * 512);
    fd1 = creat("core.0", 0777);
    write(fd1, buf1, 3 * 512);
    close(fd1);
    fd1 = creat("core", 0777);
    do {
        n = read(fd, buf, 512);
        write(fd1, buf, n);
    } while (n > 0);
}
