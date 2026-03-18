// dd -- convert and copy a file (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Supports: if=FILE, of=FILE, bs=N, count=N, skip=N, seek=N.

#include <fcntl.h>
#include <unistd.h>

namespace {

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

bool starts_with(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s != *prefix) return false;
        ++s;
        ++prefix;
    }
    return true;
}

unsigned long parse_num(const char* s) {
    unsigned long v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + static_cast<unsigned long>(*s - '0');
        ++s;
    }
    // Support suffixes: k, m, g (case-insensitive)
    if (*s == 'k' || *s == 'K') v *= 1024;
    else if (*s == 'm' || *s == 'M') v *= 1024 * 1024;
    else if (*s == 'g' || *s == 'G') v *= 1024UL * 1024 * 1024;
    return v;
}

} // namespace

int main(int argc, char** argv) {
    const char* infile = nullptr;
    const char* outfile = nullptr;
    unsigned long bs = 512;
    unsigned long count = 0;   // 0 = until EOF
    unsigned long skip = 0;
    unsigned long seek = 0;

    for (int i = 1; i < argc; ++i) {
        if (starts_with(argv[i], "if="))    infile = argv[i] + 3;
        else if (starts_with(argv[i], "of=")) outfile = argv[i] + 3;
        else if (starts_with(argv[i], "bs=")) bs = parse_num(argv[i] + 3);
        else if (starts_with(argv[i], "count=")) count = parse_num(argv[i] + 6);
        else if (starts_with(argv[i], "skip=")) skip = parse_num(argv[i] + 5);
        else if (starts_with(argv[i], "seek=")) seek = parse_num(argv[i] + 5);
    }

    if (bs == 0) bs = 512;
    if (bs > 65536) bs = 65536;  // limit stack allocation

    int fdi = 0;  // stdin
    int fdo = 1;  // stdout

    if (infile) {
        fdi = open(infile, O_RDONLY, 0);
        if (fdi < 0) {
            write_str(2, "dd: cannot open input: ");
            write_str(2, infile);
            write_str(2, "\n");
            return 1;
        }
    }

    if (outfile) {
        fdo = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fdo < 0) {
            write_str(2, "dd: cannot open output: ");
            write_str(2, outfile);
            write_str(2, "\n");
            if (infile) close(fdi);
            return 1;
        }
    }

    // Skip input blocks
    if (skip > 0) {
        char skipbuf[512];
        for (unsigned long s = 0; s < skip; ++s) {
            unsigned long to_skip = bs;
            while (to_skip > 0) {
                unsigned chunk = to_skip > sizeof(skipbuf)
                    ? sizeof(skipbuf) : static_cast<unsigned>(to_skip);
                auto r = read(fdi, skipbuf, chunk);
                if (r <= 0) break;
                to_skip -= static_cast<unsigned long>(r);
            }
        }
    }

    // Seek output blocks
    if (seek > 0) {
        lseek(fdo, static_cast<long>(seek * bs), SEEK_SET);
    }

    // Copy
    static char buf[65536];
    unsigned long records_in_full = 0;
    unsigned long records_in_partial = 0;
    unsigned long records_out_full = 0;
    unsigned long records_out_partial = 0;
    unsigned long bytes_copied = 0;
    unsigned long blocks_done = 0;

    for (;;) {
        if (count > 0 && blocks_done >= count) break;

        auto nr = read(fdi, buf, static_cast<unsigned>(bs));
        if (nr <= 0) break;

        if (static_cast<unsigned long>(nr) == bs)
            ++records_in_full;
        else
            ++records_in_partial;

        const char* p = buf;
        auto left = nr;
        while (left > 0) {
            auto nw = write(fdo, p, static_cast<unsigned>(left));
            if (nw <= 0) {
                write_str(2, "dd: write error\n");
                goto done;
            }
            p += nw;
            left -= nw;
        }

        if (static_cast<unsigned long>(nr) == bs)
            ++records_out_full;
        else
            ++records_out_partial;

        bytes_copied += static_cast<unsigned long>(nr);
        ++blocks_done;
    }

done:
    // Report to stderr
    write_ulong(2, records_in_full);
    write_str(2, "+");
    write_ulong(2, records_in_partial);
    write_str(2, " records in\n");
    write_ulong(2, records_out_full);
    write_str(2, "+");
    write_ulong(2, records_out_partial);
    write_str(2, " records out\n");
    write_ulong(2, bytes_copied);
    write_str(2, " bytes copied\n");

    if (infile) close(fdi);
    if (outfile) close(fdo);
    return 0;
}
