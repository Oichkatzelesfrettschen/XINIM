#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static char g_buf[65536];
static char *g_lines[4096];
static int g_nlines;
static int g_reverse;
static int g_numeric;

static long to_long(const char *s) {
    long val = 0;
    int neg = 0;
    while (*s == ' ' || *s == '\t') ++s;
    if (*s == '-') { neg = 1; ++s; }
    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); ++s; }
    return neg ? -val : val;
}

static int cmp_str(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    int r = strcmp(sa, sb);
    return g_reverse ? -r : r;
}

static int cmp_num(const void *a, const void *b) {
    long la = to_long(*(const char **)a);
    long lb = to_long(*(const char **)b);
    int r = (la > lb) - (la < lb);
    return g_reverse ? -r : r;
}

static void read_lines(int fd) {
    ssize_t total = 0;
    ssize_t n;
    while (total < (ssize_t)sizeof(g_buf) - 1 &&
           (n = read(fd, g_buf + total, sizeof(g_buf) - 1 - total)) > 0) {
        total += n;
    }
    g_buf[total] = '\0';

    char *p = g_buf;
    while (*p && g_nlines < 4096) {
        g_lines[g_nlines++] = p;
        char *nl = strchr(p, '\n');
        if (nl) { *nl = '\0'; p = nl + 1; }
        else break;
    }
}

int main(int argc, char **argv) {
    int first_file = 1;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            for (const char *p = argv[i]+1; *p; ++p) {
                if (*p == 'r') g_reverse = 1;
                else if (*p == 'n') g_numeric = 1;
            }
            first_file = i + 1;
        } else break;
    }

    if (first_file >= argc) {
        read_lines(0);
    } else {
        for (int i = first_file; i < argc; ++i) {
            int fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) continue;
            read_lines(fd);
            close(fd);
        }
    }

    qsort(g_lines, g_nlines, sizeof(char *), g_numeric ? cmp_num : cmp_str);

    for (int i = 0; i < g_nlines; ++i) {
        write(1, g_lines[i], strlen(g_lines[i]));
        write(1, "\n", 1);
    }
    return 0;
}
