#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* Simple tail: read entire file into buffer, find last N newlines */
static char g_buf[65536];

static int tail_fd(int fd, int lines) {
    ssize_t total = 0;
    ssize_t n;
    while (total < (ssize_t)sizeof(g_buf) - 1 &&
           (n = read(fd, g_buf + total, sizeof(g_buf) - 1 - total)) > 0) {
        total += n;
    }
    if (total == 0) return 0;

    /* Find start of last N lines */
    int count = 0;
    ssize_t pos = total - 1;
    /* Skip trailing newline */
    if (pos >= 0 && g_buf[pos] == '\n') --pos;
    while (pos >= 0 && count < lines) {
        if (g_buf[pos] == '\n') ++count;
        if (count < lines) --pos;
    }
    if (pos < 0) pos = 0;
    else if (g_buf[pos] == '\n') ++pos;

    write(1, g_buf + pos, total - pos);
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

    if (first_file >= argc) return tail_fd(0, lines);

    for (int i = first_file; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write(2, "tail: cannot open ", 18);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
            continue;
        }
        tail_fd(fd, lines);
        close(fd);
    }
    return 0;
}
