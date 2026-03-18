// file -- determine file type (simplified)
// Cleanroom C++23 implementation.
// Checks: ELF header, text/binary heuristic, directory, symlink, empty.

#include <fcntl.h>
#include <sys/stat.h>
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

bool is_printable(unsigned char c) {
    return c >= 0x20 && c < 0x7f;
}

bool is_text_char(unsigned char c) {
    return is_printable(c) || c == '\n' || c == '\r' || c == '\t' || c == '\f';
}

void identify(const char* path) {
    write_str(1, path);
    write_str(1, ": ");

    struct stat st{};
    if (lstat(path, &st) != 0) {
        write_str(1, "cannot stat\n");
        return;
    }

    // Symbolic link
    if (S_ISLNK(st.st_mode)) {
        char target[256];
        auto n = readlink(path, target, sizeof(target) - 1);
        if (n > 0) {
            target[n] = '\0';
            write_str(1, "symbolic link to ");
            write_str(1, target);
            write_str(1, "\n");
        } else {
            write_str(1, "symbolic link\n");
        }
        return;
    }

    // Directory
    if (S_ISDIR(st.st_mode)) {
        write_str(1, "directory\n");
        return;
    }

    // Character device
    if (S_ISCHR(st.st_mode)) {
        write_str(1, "character special\n");
        return;
    }

    // Block device
    if (S_ISBLK(st.st_mode)) {
        write_str(1, "block special\n");
        return;
    }

    // FIFO
    if (S_ISFIFO(st.st_mode)) {
        write_str(1, "fifo (named pipe)\n");
        return;
    }

    // Regular file -- read header
    if (!S_ISREG(st.st_mode)) {
        write_str(1, "unknown\n");
        return;
    }

    // Empty file
    if (st.st_size == 0) {
        write_str(1, "empty\n");
        return;
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        write_str(1, "cannot read\n");
        return;
    }

    unsigned char header[512];
    auto nr = read(fd, header, sizeof(header));
    close(fd);

    if (nr <= 0) {
        write_str(1, "empty\n");
        return;
    }

    // ELF check: \x7fELF
    if (nr >= 16 &&
        header[0] == 0x7f && header[1] == 'E' &&
        header[2] == 'L' && header[3] == 'F') {
        write_str(1, "ELF ");
        // Class
        if (header[4] == 1) write_str(1, "32-bit ");
        else if (header[4] == 2) write_str(1, "64-bit ");
        // Endianness
        if (header[5] == 1) write_str(1, "LSB ");
        else if (header[5] == 2) write_str(1, "MSB ");
        // Type (at offset 16 in LE)
        if (nr >= 18 && header[5] == 1) {
            unsigned type = header[16] | (static_cast<unsigned>(header[17]) << 8);
            if (type == 1) write_str(1, "relocatable");
            else if (type == 2) write_str(1, "executable");
            else if (type == 3) write_str(1, "shared object");
            else write_str(1, "object");
        } else {
            write_str(1, "object");
        }
        write_str(1, "\n");
        return;
    }

    // Shell script check: #!
    if (nr >= 2 && header[0] == '#' && header[1] == '!') {
        write_str(1, "script");
        // Try to show interpreter
        if (nr > 2) {
            int start = 2;
            while (start < static_cast<int>(nr) && header[start] == ' ') ++start;
            int end = start;
            while (end < static_cast<int>(nr) && header[end] != '\n' && header[end] != '\r')
                ++end;
            if (end > start) {
                write_str(1, ", ");
                write_all(1, reinterpret_cast<const char*>(header + start), end - start);
            }
        }
        write_str(1, "\n");
        return;
    }

    // Text/binary heuristic: if >80% of bytes are printable, call it text
    int text_chars = 0;
    int total = static_cast<int>(nr);
    for (int i = 0; i < total; ++i) {
        if (is_text_char(header[i])) ++text_chars;
    }

    if (text_chars * 100 / total > 80) {
        write_str(1, "ASCII text\n");
    } else {
        write_str(1, "data\n");
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: file file ...\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        identify(argv[i]);
    }

    return 0;
}
