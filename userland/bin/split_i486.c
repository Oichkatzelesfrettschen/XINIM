#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    int lines_per = 1000;
    const char *input = 0;
    const char *prefix = "x";
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'l' && i + 1 < argc) {
            long v = 0; const char *p = argv[++i];
            while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
            if (v > 0) lines_per = (int)v;
        } else if (!input) input = argv[i];
        else prefix = argv[i];
    }
    int fd = 0;
    if (input && !(input[0] == '-' && input[1] == '\0')) {
        fd = open(input, O_RDONLY, 0); if (fd < 0) return 1;
    }

    int filenum = 0, linecount = 0, outfd = -1;
    char ch;
    while (read(fd, &ch, 1) == 1) {
        if (outfd < 0 || linecount >= lines_per) {
            if (outfd >= 0) close(outfd);
            char name[64];
            size_t plen = strlen(prefix);
            memcpy(name, prefix, plen);
            name[plen] = (char)('a' + filenum / 26);
            name[plen+1] = (char)('a' + filenum % 26);
            name[plen+2] = '\0';
            outfd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (outfd < 0) return 1;
            ++filenum; linecount = 0;
        }
        write(outfd, &ch, 1);
        if (ch == '\n') ++linecount;
    }
    if (outfd >= 0) close(outfd);
    if (fd > 0) close(fd);
    return 0;
}
