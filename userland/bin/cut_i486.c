#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    char delim = '\t';
    int field = 0; /* 1-based field number */
    int first_file = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'd' && i + 1 < argc) {
            delim = argv[++i][0];
            first_file = i + 1;
        } else if (argv[i][0] == '-' && argv[i][1] == 'f' && i + 1 < argc) {
            long v = 0;
            for (const char *p = argv[++i]; *p >= '0' && *p <= '9'; ++p)
                v = v * 10 + (*p - '0');
            field = (int)v;
            first_file = i + 1;
        } else if (argv[i][0] != '-') { break; }
        else first_file = i + 1;
    }

    if (field <= 0) { write(2, "usage: cut -f N [-d C] [file]\n", 30); return 1; }

    int fd = 0;
    if (first_file < argc) {
        fd = open(argv[first_file], O_RDONLY, 0);
        if (fd < 0) { write(2, "cut: cannot open file\n", 22); return 1; }
    }

    char line[4096];
    ssize_t line_len = 0;
    char ch;

    while (read(fd, &ch, 1) == 1) {
        if (ch == '\n' || line_len >= (ssize_t)sizeof(line) - 1) {
            line[line_len] = '\0';
            /* extract field */
            int cur_field = 1;
            const char *start = line;
            const char *end = line;
            while (*end) {
                if (*end == delim) {
                    if (cur_field == field) break;
                    ++cur_field;
                    start = end + 1;
                }
                ++end;
            }
            if (cur_field == field) {
                const char *fend = end;
                write(1, start, fend - start);
            }
            write(1, "\n", 1);
            line_len = 0;
        } else {
            line[line_len++] = ch;
        }
    }

    if (fd != 0) close(fd);
    return 0;
}
