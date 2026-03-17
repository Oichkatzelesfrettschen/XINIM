#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static void write_num(int fd, unsigned long n) {
    char buf[20];
    int pos = 0;
    if (n == 0) { buf[pos++] = '0'; }
    else { while (n > 0) { buf[pos++] = (char)('0' + n % 10); n /= 10; } }
    for (int i = 0; i < pos / 2; ++i) {
        char t = buf[i]; buf[i] = buf[pos-1-i]; buf[pos-1-i] = t;
    }
    write(fd, buf, pos);
}

static void wc_fd(int fd, unsigned long *tl, unsigned long *tw, unsigned long *tc) {
    char buf[512];
    ssize_t n;
    int in_word = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; ++i) {
            ++(*tc);
            if (buf[i] == '\n') ++(*tl);
            if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n' ||
                buf[i] == '\r' || buf[i] == '\v' || buf[i] == '\f') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                ++(*tw);
            }
        }
    }
}

int main(int argc, char **argv) {
    int show_l = 0, show_w = 0, show_c = 0;
    int first_file = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (const char *p = argv[i]+1; *p; ++p) {
                if (*p == 'l') show_l = 1;
                else if (*p == 'w') show_w = 1;
                else if (*p == 'c') show_c = 1;
            }
        } else { break; }
        first_file = i + 1;
    }
    if (!show_l && !show_w && !show_c) { show_l = show_w = show_c = 1; }

    unsigned long lines = 0, words = 0, chars = 0;
    if (first_file >= argc) {
        wc_fd(0, &lines, &words, &chars);
    } else {
        for (int i = first_file; i < argc; ++i) {
            unsigned long fl = 0, fw = 0, fc = 0;
            int fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write(2, "wc: ", 4);
                write(2, argv[i], strlen(argv[i]));
                write(2, ": not found\n", 12);
                continue;
            }
            wc_fd(fd, &fl, &fw, &fc);
            close(fd);
            if (show_l) { write(1, " ", 1); write_num(1, fl); }
            if (show_w) { write(1, " ", 1); write_num(1, fw); }
            if (show_c) { write(1, " ", 1); write_num(1, fc); }
            write(1, " ", 1);
            write(1, argv[i], strlen(argv[i]));
            write(1, "\n", 1);
            lines += fl; words += fw; chars += fc;
        }
        if (argc - first_file > 1) {
            if (show_l) { write(1, " ", 1); write_num(1, lines); }
            if (show_w) { write(1, " ", 1); write_num(1, words); }
            if (show_c) { write(1, " ", 1); write_num(1, chars); }
            write(1, " total\n", 7);
        }
        return 0;
    }
    if (show_l) { write(1, " ", 1); write_num(1, lines); }
    if (show_w) { write(1, " ", 1); write_num(1, words); }
    if (show_c) { write(1, " ", 1); write_num(1, chars); }
    write(1, "\n", 1);
    return 0;
}
