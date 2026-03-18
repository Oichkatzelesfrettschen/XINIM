// cksum -- print CRC checksum and byte count (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Standard CRC-32 polynomial (ISO 3309 / ITU-T V.42).

#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;

void write_all(int fd, const char* buf, int len) {
    while (len > 0) {
        auto w = write(fd, buf, static_cast<unsigned>(len));
        if (w <= 0) return;
        buf += w;
        len -= static_cast<int>(w);
    }
}

void write_str(int fd, const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    write_all(fd, s, n);
}

void write_ulong(int fd, unsigned long v) {
    char tmp[20];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[20];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

// POSIX cksum uses a specific CRC polynomial different from "standard" CRC-32.
// The polynomial is 0x04C11DB7 with bit-reversal per POSIX spec.
// We build the table at startup.

unsigned long crc_table[256];

void init_crc_table() {
    for (unsigned i = 0; i < 256; ++i) {
        unsigned long c = static_cast<unsigned long>(i) << 24;
        for (int j = 0; j < 8; ++j) {
            if (c & 0x80000000UL)
                c = (c << 1) ^ 0x04C11DB7UL;
            else
                c <<= 1;
        }
        crc_table[i] = c & 0xFFFFFFFFUL;
    }
}

unsigned long crc_update(unsigned long crc, const unsigned char* buf, int len) {
    for (int i = 0; i < len; ++i) {
        unsigned idx = ((crc >> 24) ^ buf[i]) & 0xFF;
        crc = ((crc << 8) ^ crc_table[idx]) & 0xFFFFFFFFUL;
    }
    return crc;
}

// POSIX cksum appends the file length to the CRC computation
unsigned long crc_finalize(unsigned long crc, unsigned long nbytes) {
    unsigned long len = nbytes;
    while (len > 0) {
        unsigned idx = ((crc >> 24) ^ (len & 0xFF)) & 0xFF;
        crc = ((crc << 8) ^ crc_table[idx]) & 0xFFFFFFFFUL;
        len >>= 8;
    }
    return crc ^ 0xFFFFFFFFUL;
}

int cksum_fd(int fd, const char* name) {
    unsigned char buf[kBufSize];
    unsigned long crc = 0;
    unsigned long nbytes = 0;

    for (;;) {
        auto nr = read(fd, buf, sizeof(buf));
        if (nr <= 0) break;
        crc = crc_update(crc, buf, static_cast<int>(nr));
        nbytes += static_cast<unsigned long>(nr);
    }

    crc = crc_finalize(crc, nbytes);

    write_ulong(1, crc);
    write_str(1, " ");
    write_ulong(1, nbytes);
    if (name) {
        write_str(1, " ");
        write_str(1, name);
    }
    write_str(1, "\n");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    init_crc_table();

    if (argc < 2) {
        return cksum_fd(0, nullptr);
    }

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write_str(2, "cksum: ");
            write_str(2, argv[i]);
            write_str(2, ": cannot open\n");
            status = 1;
            continue;
        }
        cksum_fd(fd, argv[i]);
        close(fd);
    }
    return status;
}
