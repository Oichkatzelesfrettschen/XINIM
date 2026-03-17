#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static long parse_num(const char *s) {
    long v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); ++s; }
    if (*s == 'k' || *s == 'K') v *= 1024;
    else if (*s == 'm' || *s == 'M') v *= 1024 * 1024;
    return v;
}

int main(int argc, char **argv) {
    const char *if_path = 0;
    const char *of_path = 0;
    long bs = 512;
    long count = -1;
    long skip_blocks = 0;
    long seek_blocks = 0;

    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "if=", 3) == 0) if_path = argv[i] + 3;
        else if (strncmp(argv[i], "of=", 3) == 0) of_path = argv[i] + 3;
        else if (strncmp(argv[i], "bs=", 3) == 0) bs = parse_num(argv[i] + 3);
        else if (strncmp(argv[i], "count=", 6) == 0) count = parse_num(argv[i] + 6);
        else if (strncmp(argv[i], "skip=", 5) == 0) skip_blocks = parse_num(argv[i] + 5);
        else if (strncmp(argv[i], "seek=", 5) == 0) seek_blocks = parse_num(argv[i] + 5);
    }

    if (bs <= 0 || bs > 65536) bs = 512;

    int ifd = 0;
    int ofd = 1;

    if (if_path) {
        ifd = open(if_path, O_RDONLY, 0);
        if (ifd < 0) { write(2, "dd: cannot open input\n", 22); return 1; }
    }
    if (of_path) {
        ofd = open(of_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (ofd < 0) { write(2, "dd: cannot open output\n", 23); if (ifd > 0) close(ifd); return 1; }
    }

    /* Skip input blocks */
    if (skip_blocks > 0) {
        char skip_buf[512];
        for (long i = 0; i < skip_blocks; ++i) {
            ssize_t r = read(ifd, skip_buf, bs < (long)sizeof(skip_buf) ? bs : (long)sizeof(skip_buf));
            if (r <= 0) break;
        }
    }

    /* Seek output */
    if (seek_blocks > 0) {
        lseek(ofd, seek_blocks * bs, 0); /* SEEK_SET */
    }

    char buf[65536];
    long blocks = 0;
    long total_bytes = 0;
    ssize_t n;

    while (count < 0 || blocks < count) {
        n = read(ifd, buf, bs);
        if (n <= 0) break;
        ssize_t w = write(ofd, buf, n);
        if (w <= 0) break;
        total_bytes += w;
        ++blocks;
    }

    if (ifd > 0) close(ifd);
    if (ofd > 1) close(ofd);
    return 0;
}
