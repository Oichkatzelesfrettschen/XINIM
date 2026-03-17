#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static void write_num(int fd, unsigned long n) {
    char buf[20];
    int pos = 0;
    if (n == 0) buf[pos++] = '0';
    else while (n > 0) { buf[pos++] = (char)('0' + n % 10); n /= 10; }
    for (int i = 0; i < pos / 2; ++i) {
        char t = buf[i]; buf[i] = buf[pos-1-i]; buf[pos-1-i] = t;
    }
    write(fd, buf, pos);
}

int main(int argc, char **argv) {
    int show_count = 0, only_dupes = 0;
    int first_file = 1;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            for (const char *p = argv[i]+1; *p; ++p) {
                if (*p == 'c') show_count = 1;
                else if (*p == 'd') only_dupes = 1;
            }
            first_file = i + 1;
        } else break;
    }

    int fd = 0;
    if (first_file < argc) {
        fd = open(argv[first_file], O_RDONLY, 0);
        if (fd < 0) { write(2, "uniq: cannot open file\n", 23); return 1; }
    }

    char prev[1024] = {0};
    char line[1024];
    int prev_valid = 0;
    unsigned long count = 0;
    ssize_t line_len = 0;
    char ch;

    while (read(fd, &ch, 1) == 1) {
        if (ch == '\n' || line_len >= (ssize_t)sizeof(line) - 1) {
            line[line_len] = '\0';
            if (prev_valid && strcmp(line, prev) == 0) {
                ++count;
            } else {
                if (prev_valid && (!only_dupes || count > 1)) {
                    if (show_count) { write_num(1, count); write(1, " ", 1); }
                    write(1, prev, strlen(prev));
                    write(1, "\n", 1);
                }
                strcpy(prev, line);
                count = 1;
                prev_valid = 1;
            }
            line_len = 0;
        } else {
            line[line_len++] = ch;
        }
    }
    if (prev_valid && (!only_dupes || count > 1)) {
        if (show_count) { write_num(1, count); write(1, " ", 1); }
        write(1, prev, strlen(prev));
        write(1, "\n", 1);
    }

    if (fd != 0) close(fd);
    return 0;
}
