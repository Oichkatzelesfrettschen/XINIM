#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static char g_buf[65536];

int main(int argc, char **argv) {
    int fd = 0;
    if (argc > 1) { fd = open(argv[1], O_RDONLY, 0); if (fd < 0) return 1; }
    ssize_t total = 0, n;
    while (total < (ssize_t)sizeof(g_buf) - 1 &&
           (n = read(fd, g_buf + total, sizeof(g_buf) - 1 - total)) > 0) total += n;
    if (fd > 0) close(fd);

    char *lines[4096]; int count = 0;
    char *p = g_buf; g_buf[total] = '\0';
    while (*p && count < 4096) {
        lines[count++] = p;
        char *nl = strchr(p, '\n');
        if (nl) { *nl = '\0'; p = nl + 1; } else break;
    }
    for (int i = count - 1; i >= 0; --i) {
        write(1, lines[i], strlen(lines[i]));
        write(1, "\n", 1);
    }
    return 0;
}
