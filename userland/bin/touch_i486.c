#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        write(2, "usage: touch file ...\n", 22);
        return 1;
    }
    int status = 0;
    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_WRONLY | O_CREAT, 0644);
        if (fd < 0) {
            write(2, "touch: cannot create ", 21);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
            status = 1;
        } else {
            close(fd);
        }
    }
    return status;
}
