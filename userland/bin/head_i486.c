#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static int head_fd(int fd, int lines) {
    char buf[512];
    ssize_t n;
    int count = 0;
    while (count < lines && (n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n && count < lines; ++i) {
            write(1, buf + i, 1);
            if (buf[i] == '\n') ++count;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    int lines = 10;
    int first_file = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'n' && i + 1 < argc) {
            long val = 0;
            for (const char *p = argv[++i]; *p >= '0' && *p <= '9'; ++p)
                val = val * 10 + (*p - '0');
            if (val > 0) lines = (int)val;
            first_file = i + 1;
        } else if (argv[i][0] != '-') { break; }
        else first_file = i + 1;
    }

    if (first_file >= argc) return head_fd(0, lines);

    for (int i = first_file; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write(2, "head: cannot open ", 18);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
            continue;
        }
        if (argc - first_file > 1) {
            write(1, "==> ", 4);
            write(1, argv[i], strlen(argv[i]));
            write(1, " <==\n", 5);
        }
        head_fd(fd, lines);
        close(fd);
    }
    return 0;
}
