#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* POSIX cksum: CRC-32 + byte count */
static unsigned int crc_table[256];
static int table_ready = 0;

static void make_table(void) {
    for (unsigned int i = 0; i < 256; ++i) {
        unsigned int c = i << 24;
        for (int j = 0; j < 8; ++j)
            c = (c & 0x80000000U) ? (c << 1) ^ 0x04C11DB7U : c << 1;
        crc_table[i] = c;
    }
    table_ready = 1;
}

static void write_num(int fd, unsigned long n) {
    char buf[20]; int pos = 0;
    if (n == 0) buf[pos++] = '0';
    else while (n > 0) { buf[pos++] = (char)('0' + n % 10); n /= 10; }
    for (int i = 0; i < pos/2; ++i) { char t=buf[i]; buf[i]=buf[pos-1-i]; buf[pos-1-i]=t; }
    write(fd, buf, pos);
}

int main(int argc, char **argv) {
    if (!table_ready) make_table();
    int start = (argc > 1) ? 1 : 0;
    int nfiles = (argc > 1) ? argc - 1 : 1;

    for (int fi = 0; fi < nfiles; ++fi) {
        int fd = 0;
        const char *name = "";
        if (start > 0) {
            name = argv[start + fi];
            fd = open(name, O_RDONLY, 0);
            if (fd < 0) { write(2, "cksum: cannot open ", 19); write(2, name, strlen(name)); write(2, "\n",1); continue; }
        }
        unsigned int crc = 0;
        unsigned long bytes = 0;
        unsigned char buf[4096];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            for (ssize_t i = 0; i < n; ++i)
                crc = (crc << 8) ^ crc_table[((crc >> 24) ^ buf[i]) & 0xFF];
            bytes += (unsigned long)n;
        }
        /* Fold in length */
        unsigned long len = bytes;
        while (len > 0) { crc = (crc << 8) ^ crc_table[((crc >> 24) ^ (len & 0xFF)) & 0xFF]; len >>= 8; }
        crc = ~crc;

        write_num(1, crc);
        write(1, " ", 1);
        write_num(1, bytes);
        if (start > 0) { write(1, " ", 1); write(1, name, strlen(name)); }
        write(1, "\n", 1);
        if (fd > 0) close(fd);
    }
    return 0;
}
