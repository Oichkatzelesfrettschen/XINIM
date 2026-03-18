// du -- estimate file space usage (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Walks arguments with stat() and reports 512-byte block counts.
// Supports -s (summary only).

#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

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

void write_uint(int fd, unsigned long v) {
    char tmp[20];
    int n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[20];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

// Build a path from directory prefix and entry name
int path_join(char* out, int out_size, const char* dir, const char* name) {
    int dlen = 0;
    while (dir[dlen] != '\0') ++dlen;
    int nlen = 0;
    while (name[nlen] != '\0') ++nlen;

    int total = dlen + 1 + nlen + 1; // dir + '/' + name + '\0'
    if (total > out_size) return -1;

    for (int i = 0; i < dlen; ++i) out[i] = dir[i];
    if (dlen > 0 && dir[dlen - 1] != '/') {
        out[dlen] = '/';
        ++dlen;
    }
    for (int i = 0; i < nlen; ++i) out[dlen + i] = name[i];
    out[dlen + nlen] = '\0';
    return 0;
}

// Minimal directory entry reading via getdents syscall
struct linux_dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    char d_name[1]; // variable length
};

long sys_getdents(int fd, void* buf, unsigned count) {
    long result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(141), "b"(fd), "c"(buf), "d"(count)
        : "memory", "cc");
    return result;
}

// Recursively compute disk usage in 512-byte blocks
unsigned long du_path(const char* path, bool summary) {
    struct stat st{};
    if (stat(path, &st) != 0) {
        write_str(2, "du: cannot stat ");
        write_str(2, path);
        write_str(2, "\n");
        return 0;
    }

    unsigned long blocks = (static_cast<unsigned long>(st.st_size) + 511) / 512;

    if (S_ISDIR(st.st_mode)) {
        int fd = open(path, O_RDONLY, 0);
        if (fd < 0) return blocks;

        char dbuf[1024];
        for (;;) {
            auto nread = sys_getdents(fd, dbuf, sizeof(dbuf));
            if (nread <= 0) break;

            int pos = 0;
            while (pos < static_cast<int>(nread)) {
                auto* ent = reinterpret_cast<linux_dirent*>(dbuf + pos);
                const char* name = ent->d_name;

                // Skip . and ..
                if (name[0] == '.' && (name[1] == '\0' ||
                    (name[1] == '.' && name[2] == '\0'))) {
                    pos += ent->d_reclen;
                    continue;
                }

                char child[512];
                if (path_join(child, static_cast<int>(sizeof(child)), path, name) == 0) {
                    blocks += du_path(child, summary);
                }
                pos += ent->d_reclen;
            }
        }
        close(fd);
    }

    if (!summary) {
        write_uint(1, blocks);
        write_str(1, "\t");
        write_str(1, path);
        write_str(1, "\n");
    }

    return blocks;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: du [-s] file ...\n");
        return 1;
    }

    bool summary = false;
    int first = 1;
    if (argv[1][0] == '-' && argv[1][1] == 's' && argv[1][2] == '\0') {
        summary = true;
        first = 2;
    }

    if (first >= argc) {
        write_str(2, "usage: du [-s] file ...\n");
        return 1;
    }

    unsigned long total = 0;
    for (int i = first; i < argc; ++i) {
        unsigned long blocks = du_path(argv[i], summary);
        total += blocks;
        if (summary) {
            write_uint(1, blocks);
            write_str(1, "\t");
            write_str(1, argv[i]);
            write_str(1, "\n");
        }
    }

    if (argc - first > 1) {
        write_uint(1, total);
        write_str(1, "\ttotal\n");
    }
    return 0;
}
