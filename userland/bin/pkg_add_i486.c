#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Minimal package installer for XINIM pkgsrc Track A.
 * Extracts a tar archive to /usr/pkg/ and records the package
 * name in /var/db/pkg/<name>/+CONTENTS.
 *
 * Usage: pkg_add /packages/bmake.tar
 */

static void write_str(const char *s) { write(1, s, strlen(s)); }

static const char *basename_of(const char *path) {
    const char *p = path;
    const char *last = path;
    while (*p) { if (*p == '/' && p[1]) last = p + 1; ++p; }
    return last;
}

static void strip_tar_ext(const char *name, char *out, int cap) {
    int len = (int)strlen(name);
    /* Strip .tar, .tar.gz, .tgz */
    if (len > 4 && strcmp(name + len - 4, ".tar") == 0) len -= 4;
    else if (len > 7 && strcmp(name + len - 7, ".tar.gz") == 0) len -= 7;
    else if (len > 4 && strcmp(name + len - 4, ".tgz") == 0) len -= 4;
    if (len >= cap) len = cap - 1;
    memcpy(out, name, (size_t)len);
    out[len] = '\0';
}

/* Minimal tar extract inline (same logic as tar_i486.c) */
struct TarHdr {
    char name[100]; char mode[8]; char uid[8]; char gid[8];
    char size[12]; char mtime[12]; char chksum[8]; char typeflag;
    char linkname[100]; char magic[6]; char version[2];
    char uname[32]; char gname[32]; char devmajor[8]; char devminor[8];
    char prefix[155]; char pad[12];
};

static unsigned long oct(const char *s, int n) {
    unsigned long v = 0;
    for (int i = 0; i < n && s[i] >= '0' && s[i] <= '7'; ++i)
        v = v * 8 + (unsigned long)(s[i] - '0');
    return v;
}

static int is_zero(const char *b) {
    for (int i = 0; i < 512; ++i) if (b[i]) return 0;
    return 1;
}

static void mkdirs(const char *path) {
    char buf[256]; size_t len = strlen(path);
    if (len >= sizeof(buf)) return;
    memcpy(buf, path, len + 1);
    for (size_t i = 1; i < len; ++i) {
        if (buf[i] == '/') { buf[i] = '\0'; mkdir(buf, 0755); buf[i] = '/'; }
    }
}

static int extract_tar(const char *archive, const char *dest_prefix) {
    int fd = open(archive, O_RDONLY, 0);
    if (fd < 0) return -1;

    char block[512];
    int zeros = 0, count = 0;
    while (read(fd, block, 512) == 512) {
        if (is_zero(block)) { if (++zeros >= 2) break; continue; }
        zeros = 0;
        struct TarHdr *h = (struct TarHdr *)block;
        unsigned long sz = oct(h->size, 12);

        /* Build output path: dest_prefix + "/" + name */
        char outpath[512];
        int plen = (int)strlen(dest_prefix);
        memcpy(outpath, dest_prefix, (size_t)plen);
        if (plen > 0 && dest_prefix[plen-1] != '/') outpath[plen++] = '/';
        size_t nlen = strlen(h->name);
        if ((size_t)plen + nlen + 1 > sizeof(outpath)) { /* skip */ }
        else {
            memcpy(outpath + plen, h->name, nlen + 1);

            if (h->typeflag == '5' || (h->typeflag == '\0' && outpath[plen + nlen - 1] == '/')) {
                mkdirs(outpath);
                mkdir(outpath, 0755);
            } else if (h->typeflag == '0' || h->typeflag == '\0') {
                mkdirs(outpath);
                int out = open(outpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (out >= 0) {
                    unsigned long rem = sz;
                    while (rem > 0) {
                        if (read(fd, block, 512) != 512) break;
                        unsigned long chunk = rem < 512 ? rem : 512;
                        write(out, block, chunk);
                        rem -= chunk;
                    }
                    close(out);
                    ++count;
                } else {
                    unsigned long blks = (sz + 511) / 512;
                    for (unsigned long b = 0; b < blks; ++b) read(fd, block, 512);
                }
            } else {
                unsigned long blks = (sz + 511) / 512;
                for (unsigned long b = 0; b < blks; ++b) read(fd, block, 512);
            }
        }
    }
    close(fd);
    return count;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        write_str("usage: pkg_add package.tar\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        const char *pkg = argv[i];
        const char *bname = basename_of(pkg);
        char pkgname[64];
        strip_tar_ext(bname, pkgname, sizeof(pkgname));

        write_str("Installing ");
        write_str(pkgname);
        write_str(" from ");
        write_str(pkg);
        write_str("...\n");

        int count = extract_tar(pkg, "/usr/pkg");
        if (count < 0) {
            write_str("pkg_add: cannot open ");
            write_str(pkg);
            write_str("\n");
            return 1;
        }

        /* Record package in /var/db/pkg/<name>/+CONTENTS */
        char dbdir[128] = "/var/db/pkg/";
        size_t dlen = strlen(dbdir);
        size_t nlen2 = strlen(pkgname);
        if (dlen + nlen2 < sizeof(dbdir)) {
            memcpy(dbdir + dlen, pkgname, nlen2 + 1);
        }
        mkdir("/var", 0755);
        mkdir("/var/db", 0755);
        mkdir("/var/db/pkg", 0755);
        mkdir(dbdir, 0755);

        char contents[160];
        memcpy(contents, dbdir, strlen(dbdir));
        memcpy(contents + strlen(dbdir), "/+CONTENTS", 11);
        int cfd = open(contents, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (cfd >= 0) {
            write(cfd, "@name ", 6);
            write(cfd, pkgname, strlen(pkgname));
            write(cfd, "\n", 1);
            close(cfd);
        }

        char num[12]; int pos = 0;
        if (count == 0) num[pos++] = '0';
        else { int v = count; while (v > 0) { num[pos++] = (char)('0' + v % 10); v /= 10; } }
        for (int j = 0; j < pos/2; ++j) { char t=num[j]; num[j]=num[pos-1-j]; num[pos-1-j]=t; }

        write_str("  ");
        write(1, num, pos);
        write_str(" files extracted to /usr/pkg\n");
    }
    return 0;
}
