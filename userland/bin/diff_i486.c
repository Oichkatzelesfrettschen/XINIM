#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* Minimal diff: line-by-line comparison, simple change detection */

static char g_buf1[32768];
static char g_buf2[32768];
static char *g_lines1[2048];
static char *g_lines2[2048];
static int g_count1;
static int g_count2;

static int read_lines(int fd, char *buf, int bufsz, char **lines, int maxlines) {
    ssize_t total = 0;
    ssize_t n;
    while (total < bufsz - 1 && (n = read(fd, buf + total, bufsz - 1 - total)) > 0)
        total += n;
    buf[total] = '\0';
    int count = 0;
    char *p = buf;
    while (*p && count < maxlines) {
        lines[count++] = p;
        char *nl = strchr(p, '\n');
        if (nl) { *nl = '\0'; p = nl + 1; }
        else break;
    }
    return count;
}

static void write_num(int fd, int n) {
    char buf[12]; int pos = 0;
    if (n == 0) buf[pos++] = '0';
    else { int v = n; while (v > 0) { buf[pos++] = (char)('0' + v % 10); v /= 10; } }
    for (int i = 0; i < pos/2; ++i) { char t=buf[i]; buf[i]=buf[pos-1-i]; buf[pos-1-i]=t; }
    write(fd, buf, pos);
}

int main(int argc, char **argv) {
    if (argc < 3) { write(2, "usage: diff file1 file2\n", 24); return 2; }

    int fd1 = open(argv[1], O_RDONLY, 0);
    if (fd1 < 0) { write(2, "diff: cannot open ", 18); write(2, argv[1], strlen(argv[1])); write(2, "\n", 1); return 2; }
    int fd2 = open(argv[2], O_RDONLY, 0);
    if (fd2 < 0) { close(fd1); write(2, "diff: cannot open ", 18); write(2, argv[2], strlen(argv[2])); write(2, "\n", 1); return 2; }

    g_count1 = read_lines(fd1, g_buf1, sizeof(g_buf1), g_lines1, 2048);
    g_count2 = read_lines(fd2, g_buf2, sizeof(g_buf2), g_lines2, 2048);
    close(fd1);
    close(fd2);

    /* Simple line-by-line comparison (not LCS-based, just shows changes) */
    int i = 0, j = 0;
    int has_diff = 0;
    while (i < g_count1 || j < g_count2) {
        if (i < g_count1 && j < g_count2 && strcmp(g_lines1[i], g_lines2[j]) == 0) {
            ++i; ++j;
            continue;
        }
        has_diff = 1;
        /* Look ahead to find resync point */
        int found = 0;
        for (int ahead = 1; ahead < 8 && !found; ++ahead) {
            if (i + ahead < g_count1 && j < g_count2 &&
                strcmp(g_lines1[i + ahead], g_lines2[j]) == 0) {
                /* Lines deleted from file1 */
                write_num(1, i + 1); write(1, ",", 1); write_num(1, i + ahead);
                write(1, "d", 1); write_num(1, j); write(1, "\n", 1);
                for (int k = i; k < i + ahead; ++k) {
                    write(1, "< ", 2); write(1, g_lines1[k], strlen(g_lines1[k])); write(1, "\n", 1);
                }
                i += ahead;
                found = 1;
            }
            if (j + ahead < g_count2 && i < g_count1 &&
                strcmp(g_lines1[i], g_lines2[j + ahead]) == 0) {
                /* Lines added in file2 */
                write_num(1, i); write(1, "a", 1);
                write_num(1, j + 1); write(1, ",", 1); write_num(1, j + ahead);
                write(1, "\n", 1);
                for (int k = j; k < j + ahead; ++k) {
                    write(1, "> ", 2); write(1, g_lines2[k], strlen(g_lines2[k])); write(1, "\n", 1);
                }
                j += ahead;
                found = 1;
            }
        }
        if (!found) {
            /* Changed line */
            write_num(1, i + 1); write(1, "c", 1); write_num(1, j + 1); write(1, "\n", 1);
            if (i < g_count1) { write(1, "< ", 2); write(1, g_lines1[i], strlen(g_lines1[i])); write(1, "\n", 1); }
            write(1, "---\n", 4);
            if (j < g_count2) { write(1, "> ", 2); write(1, g_lines2[j], strlen(g_lines2[j])); write(1, "\n", 1); }
            if (i < g_count1) ++i;
            if (j < g_count2) ++j;
        }
    }

    return has_diff ? 1 : 0;
}
