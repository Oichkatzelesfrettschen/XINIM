// XINIM-owned userspace implementation; compile as freestanding C++23.
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

extern "C" int xinim_user_main(void) asm("main");

extern "C" int xinim_user_main(void) {
    char cwd[16];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        puts("diethello: getcwd failed");
        return 1;
    }

    printf("diethello: cwd=%s\n", cwd);
    printf("diethello: pid=%d\n", (int)getpid());

    int fd = open("/etc/motd", O_RDONLY, 0);
    if (fd < 0) {
        puts("diethello: open failed");
        return 2;
    }

    char buffer[32];
    const ssize_t count = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (count < 0) {
        puts("diethello: read failed");
        return 3;
    }
    buffer[count] = '\0';

    printf("diethello: motd=%s\n", buffer);
    return 0;
}
