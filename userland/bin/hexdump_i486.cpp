// hexdump -- display file contents in hexadecimal + ASCII
// Cleanroom C++23 implementation.
// 16 bytes per line, offset on left.

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

void write_hex_byte(char* out, unsigned char b) {
    out[0] = "0123456789abcdef"[b >> 4];
    out[1] = "0123456789abcdef"[b & 0x0f];
}

void write_hex_offset(char* out, unsigned long off) {
    // 8-digit hex offset
    for (int i = 7; i >= 0; --i) {
        out[i] = "0123456789abcdef"[off & 0x0f];
        off >>= 4;
    }
}

void dump_line(unsigned long offset, const unsigned char* data, int len) {
    // Format: "XXXXXXXX  HH HH HH HH HH HH HH HH  HH HH HH HH HH HH HH HH  |................|\n"
    char line[80];
    // Fill with spaces
    for (int i = 0; i < 79; ++i) line[i] = ' ';

    // Offset
    write_hex_offset(line, offset);

    // Hex bytes
    for (int i = 0; i < 16; ++i) {
        int pos = 10 + i * 3;
        if (i >= 8) ++pos; // extra space between groups
        if (i < len) {
            write_hex_byte(line + pos, data[i]);
        }
    }

    // ASCII representation
    int ascii_start = 10 + 16 * 3 + 2;
    line[ascii_start - 1] = '|';
    for (int i = 0; i < 16; ++i) {
        if (i < len) {
            unsigned char c = data[i];
            line[ascii_start + i] = (c >= 0x20 && c < 0x7f)
                ? static_cast<char>(c) : '.';
        } else {
            line[ascii_start + i] = ' ';
        }
    }
    line[ascii_start + 16] = '|';
    line[ascii_start + 17] = '\n';

    write_all(1, line, ascii_start + 18);
}

int hexdump_fd(int fd) {
    unsigned char buf[16];
    unsigned long offset = 0;

    for (;;) {
        int total = 0;
        while (total < 16) {
            auto nr = read(fd, buf + total, static_cast<unsigned>(16 - total));
            if (nr <= 0) break;
            total += static_cast<int>(nr);
        }
        if (total == 0) break;
        dump_line(offset, buf, total);
        offset += static_cast<unsigned long>(total);
        if (total < 16) break;
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return hexdump_fd(0);
    }

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write_str(2, "hexdump: ");
            write_str(2, argv[i]);
            write_str(2, ": cannot open\n");
            status = 1;
            continue;
        }
        hexdump_fd(fd);
        close(fd);
    }
    return status;
}
