#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

/* Minimal tar extract (POSIX ustar format, no compression) */

struct TarHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};

static unsigned long octal_to_ulong(const char *s, int len) {
    unsigned long val = 0;
    for (int i = 0; i < len && s[i] >= '0' && s[i] <= '7'; ++i)
        val = val * 8 + (unsigned long)(s[i] - '0');
    return val;
}

static int is_zero_block(const char *block) {
    for (int i = 0; i < 512; ++i) if (block[i] != 0) return 0;
    return 1;
}

static void make_parent_dirs(const char *path) {
    char buf[256];
    size_t len = strlen(path);
    if (len >= sizeof(buf)) return;
    memcpy(buf, path, len + 1);
    for (size_t i = 1; i < len; ++i) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            mkdir(buf, 0755);
            buf[i] = '/';
        }
    }
}

int main(int argc, char **argv) {
    int extract = 0;
    const char *archive = 0;
    int verbose = 0;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            for (const char *p = argv[i]; *p; ++p) {
                if (*p == 'x') extract = 1;
                else if (*p == 'v') verbose = 1;
                else if (*p == 'f' && i + 1 < argc) archive = argv[++i];
            }
        } else if (argv[i][0] == 'x') {
            extract = 1;
            for (const char *p = argv[i]; *p; ++p) {
                if (*p == 'v') verbose = 1;
                if (*p == 'f' && i + 1 < argc) archive = argv[++i];
            }
        }
    }

    if (!extract || !archive) {
        write(2, "usage: tar xf archive.tar [-v]\n", 30);
        return 1;
    }

    int fd = open(archive, O_RDONLY, 0);
    if (fd < 0) {
        write(2, "tar: cannot open ", 17);
        write(2, archive, strlen(archive));
        write(2, "\n", 1);
        return 1;
    }

    char block[512];
    int zero_count = 0;

    while (read(fd, block, 512) == 512) {
        if (is_zero_block(block)) {
            if (++zero_count >= 2) break;
            continue;
        }
        zero_count = 0;

        struct TarHeader *hdr = (struct TarHeader *)block;
        unsigned long size = octal_to_ulong(hdr->size, 12);

        /* Build full path (prefix + name) */
        char path[256];
        int plen = 0;
        if (hdr->prefix[0] != '\0') {
            size_t pfxlen = strlen(hdr->prefix);
            if (pfxlen < sizeof(path) - 2) {
                memcpy(path, hdr->prefix, pfxlen);
                path[pfxlen] = '/';
                plen = (int)pfxlen + 1;
            }
        }
        size_t nlen = strlen(hdr->name);
        if (plen + nlen < sizeof(path)) {
            memcpy(path + plen, hdr->name, nlen + 1);
        } else {
            continue;
        }

        if (verbose) {
            write(1, path, strlen(path));
            write(1, "\n", 1);
        }

        if (hdr->typeflag == '5' || (hdr->typeflag == '\0' && path[strlen(path)-1] == '/')) {
            /* Directory */
            make_parent_dirs(path);
            mkdir(path, 0755);
        } else if (hdr->typeflag == '0' || hdr->typeflag == '\0') {
            /* Regular file */
            make_parent_dirs(path);
            int out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out < 0) {
                /* Skip data blocks */
                unsigned long blocks = (size + 511) / 512;
                for (unsigned long b = 0; b < blocks; ++b) read(fd, block, 512);
                continue;
            }
            unsigned long remaining = size;
            while (remaining > 0) {
                if (read(fd, block, 512) != 512) break;
                unsigned long chunk = remaining < 512 ? remaining : 512;
                write(out, block, chunk);
                remaining -= chunk;
            }
            close(out);
        } else {
            /* Skip unknown types */
            unsigned long blocks = (size + 511) / 512;
            for (unsigned long b = 0; b < blocks; ++b) read(fd, block, 512);
        }
    }

    close(fd);
    return 0;
}
