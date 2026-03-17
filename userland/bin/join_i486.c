#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* Simplified join: merge sorted files on common first field */
static char g_buf1[32768], g_buf2[32768];
static char *g_lines1[2048], *g_lines2[2048];
static int g_n1, g_n2;

static int read_lines(int fd, char *buf, int sz, char **lines, int max) {
    int total = 0; ssize_t n;
    while (total < sz - 1 && (n = read(fd, buf + total, sz - 1 - total)) > 0) total += (int)n;
    buf[total] = '\0';
    int count = 0; char *p = buf;
    while (*p && count < max) { lines[count++] = p; char *nl = strchr(p, '\n'); if (nl) { *nl = '\0'; p = nl + 1; } else break; }
    return count;
}

static const char *field1(const char *line) { return line; }
static int field1_len(const char *line) {
    int len = 0; while (line[len] && line[len] != ' ' && line[len] != '\t') ++len; return len;
}

int main(int argc, char **argv) {
    if (argc < 3) { write(2, "usage: join file1 file2\n", 24); return 1; }
    int fd1 = open(argv[1], O_RDONLY, 0); if (fd1 < 0) return 1;
    int fd2 = open(argv[2], O_RDONLY, 0); if (fd2 < 0) { close(fd1); return 1; }
    g_n1 = read_lines(fd1, g_buf1, sizeof(g_buf1), g_lines1, 2048); close(fd1);
    g_n2 = read_lines(fd2, g_buf2, sizeof(g_buf2), g_lines2, 2048); close(fd2);

    int i = 0, j = 0;
    while (i < g_n1 && j < g_n2) {
        int l1 = field1_len(g_lines1[i]), l2 = field1_len(g_lines2[j]);
        int cmp = (l1 == l2) ? strncmp(field1(g_lines1[i]), field1(g_lines2[j]), (size_t)l1) : (l1 < l2 ? -1 : 1);
        if (cmp == 0) {
            write(1, g_lines1[i], strlen(g_lines1[i]));
            const char *rest2 = g_lines2[j] + l2;
            while (*rest2 == ' ' || *rest2 == '\t') ++rest2;
            if (*rest2) { write(1, " ", 1); write(1, rest2, strlen(rest2)); }
            write(1, "\n", 1);
            ++i; ++j;
        } else if (cmp < 0) ++i;
        else ++j;
    }
    return 0;
}
