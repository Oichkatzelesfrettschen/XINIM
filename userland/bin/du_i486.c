#include <sys/stat.h>
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
    if (argc < 2) { write(2, "usage: du [-s] file ...\n", 23); return 1; }
    int summary = 0;
    int first = 1;
    if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == 's') {
        summary = 1;
        first = 2;
    }
    (void)summary;
    unsigned long total = 0;
    for (int i = first; i < argc; ++i) {
        struct stat st;
        if (stat(argv[i], &st) != 0) {
            write(2, "du: cannot stat ", 16);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
            continue;
        }
        unsigned long blocks = ((unsigned long)st.st_size + 511) / 512;
        total += blocks;
        write_num(1, blocks);
        write(1, "\t", 1);
        write(1, argv[i], strlen(argv[i]));
        write(1, "\n", 1);
    }
    if (argc - first > 1) {
        write_num(1, total);
        write(1, "\ttotal\n", 7);
    }
    return 0;
}
