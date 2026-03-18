// pkg_add -- XINIM pkgsrc package installer
// Cleanroom C++23 implementation.
// Extracts a ustar tar archive to /usr/pkg/ and records the package in /var/db/pkg/.
// Usage: pkg_add [-d destdir] package.tar [package2.tar ...]

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace {

void write_all(int fd, const char* buf, int len) noexcept {
    while (len > 0) {
        auto w = write(fd, buf, static_cast<unsigned>(len));
        if (w <= 0) return;
        buf += w; len -= static_cast<int>(w);
    }
}

void write_str(int fd, const char* s) noexcept {
    int n = 0; while (s[n]) ++n; write_all(fd, s, n);
}

void write_uint(int fd, unsigned long v) noexcept {
    char tmp[24]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[24];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

int str_len(const char* s) noexcept { int n = 0; while (s[n]) ++n; return n; }

void str_copy(char* dst, const char* src, int cap) noexcept {
    int i = 0;
    while (i < cap - 1 && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

bool str_ends_with(const char* s, const char* suffix) noexcept {
    int slen = str_len(s), suflen = str_len(suffix);
    if (suflen > slen) return false;
    const char* p = s + slen - suflen;
    while (*p && *suffix && *p == *suffix) { ++p; ++suffix; }
    return *p == *suffix;
}

void mem_copy(void* dst, const void* src, int n) noexcept {
    auto* d = static_cast<char*>(dst);
    const auto* s = static_cast<const char*>(src);
    for (int i = 0; i < n; ++i) d[i] = s[i];
}

void mem_zero(void* dst, int n) noexcept {
    auto* d = static_cast<char*>(dst);
    for (int i = 0; i < n; ++i) d[i] = '\0';
}

// Get base name of path.
const char* path_basename(const char* path) noexcept {
    const char* last = path;
    while (*path) { if (*path == '/' && path[1]) last = path + 1; ++path; }
    return last;
}

// Strip tar/tgz extension from package name.
void strip_pkg_ext(const char* name, char* out, int cap) noexcept {
    int len = str_len(name);
    if (len > 7 && str_ends_with(name, ".tar.gz")) len -= 7;
    else if (len > 4 && str_ends_with(name, ".tgz"))  len -= 4;
    else if (len > 4 && str_ends_with(name, ".tar"))  len -= 4;
    if (len >= cap) len = cap - 1;
    for (int i = 0; i < len; ++i) out[i] = name[i];
    out[len] = '\0';
}

// Create directory and all parents.
void mkdirs(const char* path) noexcept {
    char buf[512]; int len = str_len(path);
    if (len >= 512) return;
    str_copy(buf, path, 512);
    for (int i = 1; i < len; ++i) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            mkdir(buf, 0755);
            buf[i] = '/';
        }
    }
    mkdir(buf, 0755);
}

// ============================================================
// Embedded ustar reader (same on-disk layout as tar_i486.cpp)
// Per POSIX.1-1988 ustar specification.
// ============================================================

struct UstarHdr {
    char name[100]; char mode[8]; char uid[8]; char gid[8];
    char size[12]; char mtime[12]; char checksum[8]; char typeflag;
    char linkname[100]; char magic[6]; char version[2];
    char uname[32]; char gname[32]; char devmajor[8]; char devminor[8];
    char prefix[155]; char pad[12];
};
static_assert(sizeof(UstarHdr) == 512, "ustar header must be 512 bytes");

unsigned long read_octal(const char* s, int len) noexcept {
    unsigned long v = 0;
    for (int i = 0; i < len && s[i] >= '0' && s[i] <= '7'; ++i)
        v = v * 8U + static_cast<unsigned long>(s[i] - '0');
    return v;
}

bool is_zero_block(const char* block) noexcept {
    for (int i = 0; i < 512; ++i) if (block[i] != '\0') return false;
    return true;
}

// Build the full extraction path: destdir + "/" + prefix + "/" + name
bool build_extract_path(const UstarHdr* h, const char* destdir, char* out, int cap) noexcept {
    int pos = 0;
    auto append = [&](const char* s) {
        while (*s && pos < cap - 1) out[pos++] = *s++;
    };
    append(destdir);
    if (pos > 0 && out[pos - 1] != '/') out[pos++] = '/';
    if (h->prefix[0] != '\0') { append(h->prefix); out[pos++] = '/'; }
    append(h->name);
    out[pos] = '\0';
    return pos < cap;
}

struct ExtractResult { int files; int dirs; int errors; };

ExtractResult extract_tar(int fd, const char* destdir, bool verbose) noexcept {
    ExtractResult r{};
    char block[512]; int zero_count = 0;

    while (read(fd, block, 512) == 512) {
        if (is_zero_block(block)) {
            if (++zero_count >= 2) break;
            continue;
        }
        zero_count = 0;

        auto* h = reinterpret_cast<UstarHdr*>(block);
        unsigned long size = read_octal(h->size, 12);
        unsigned long mode = read_octal(h->mode, 8);

        char path[512];
        if (!build_extract_path(h, destdir, path, sizeof(path))) {
            // Skip data for oversized paths
            unsigned long blks = (size + 511U) / 512U;
            for (unsigned long b = 0; b < blks; ++b) {
                char discard[512]; static_cast<void>(read(fd, discard, 512));
            }
            ++r.errors;
            continue;
        }

        char typeflag = h->typeflag;
        if (typeflag == '\0') typeflag = '0';

        if (typeflag == '5' || (typeflag == '0' && path[str_len(path) - 1] == '/')) {
            mkdirs(path);
            if (verbose) { write_str(1, "  dir  "); write_str(1, path); write_str(1, "\n"); }
            ++r.dirs;
        } else if (typeflag == '0' || typeflag == '7') {
            // Regular file
            mkdirs(path); // create parent dirs
            // Trim trailing slash from file path if it got one
            int plen = str_len(path);
            if (plen > 0 && path[plen - 1] == '/') { path[plen - 1] = '\0'; }

            int outfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, static_cast<int>(mode & 0777U));
            if (outfd < 0) {
                write_str(2, "pkg_add: cannot create: "); write_str(2, path); write_str(2, "\n");
                unsigned long blks = (size + 511U) / 512U;
                for (unsigned long b = 0; b < blks; ++b) {
                    char discard[512]; static_cast<void>(read(fd, discard, 512));
                }
                ++r.errors;
                continue;
            }
            unsigned long remaining = size;
            while (remaining > 0) {
                char data[512];
                if (read(fd, data, 512) != 512) break;
                unsigned long chunk = remaining < 512U ? remaining : 512U;
                write_all(outfd, data, static_cast<int>(chunk));
                remaining -= chunk;
            }
            close(outfd);
            if (verbose) { write_str(1, "  file "); write_str(1, path); write_str(1, "\n"); }
            ++r.files;
        } else if (typeflag == '2') {
            // Symlink
            symlink(h->linkname, path);
            if (verbose) { write_str(1, "  link "); write_str(1, path); write_str(1, "\n"); }
        } else {
            // Skip other types (device nodes, etc.)
            unsigned long blks = (size + 511U) / 512U;
            for (unsigned long b = 0; b < blks; ++b) {
                char discard[512]; static_cast<void>(read(fd, discard, 512));
            }
        }
    }
    return r;
}

// ============================================================
// Package database: /var/db/pkg/<pkgname>/+CONTENTS
// Records package name and file count per pkgsrc convention.
// ============================================================

void record_package(const char* pkgname, const ExtractResult& r,
                    const char* archive_path) noexcept {
    // Ensure /var/db/pkg/<pkgname>/ exists
    char dbdir[256] = "/var/db/pkg/";
    int dlen = str_len(dbdir);
    int nlen = str_len(pkgname);
    if (dlen + nlen < 255) {
        for (int i = 0; i < nlen; ++i) dbdir[dlen + i] = pkgname[i];
        dbdir[dlen + nlen] = '\0';
    }
    mkdirs(dbdir);

    // Write +CONTENTS file
    char contents_path[280];
    str_copy(contents_path, dbdir, 256);
    int cp_len = str_len(contents_path);
    str_copy(contents_path + cp_len, "/+CONTENTS", 280 - cp_len);

    int cfd = open(contents_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (cfd < 0) return;

    // pkgsrc +CONTENTS format
    write_str(cfd, "@name ");        write_str(cfd, pkgname); write_str(cfd, "\n");
    write_str(cfd, "@archive ");     write_str(cfd, archive_path); write_str(cfd, "\n");
    write_str(cfd, "@files ");
    write_uint(cfd, static_cast<unsigned long>(r.files));
    write_str(cfd, "\n");
    write_str(cfd, "@dirs ");
    write_uint(cfd, static_cast<unsigned long>(r.dirs));
    write_str(cfd, "\n");
    close(cfd);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: pkg_add [-d destdir] [-v] package.tar [...]\n");
        return 1;
    }

    const char* destdir = "/usr/pkg";
    bool verbose = false;
    int first_pkg = 1;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'd' && i + 1 < argc) {
            destdir = argv[++i]; first_pkg = i + 1;
        } else if (argv[i][0] == '-' && argv[i][1] == 'v') {
            verbose = true; first_pkg = i + 1;
        } else {
            first_pkg = i; break;
        }
    }

    if (first_pkg >= argc) {
        write_str(2, "pkg_add: no package files specified\n");
        return 1;
    }

    // Ensure destination exists
    mkdirs(destdir);

    int status = 0;
    for (int i = first_pkg; i < argc; ++i) {
        const char* pkg_path = argv[i];
        const char* bname = path_basename(pkg_path);
        char pkgname[128];
        strip_pkg_ext(bname, pkgname, sizeof(pkgname));

        write_str(1, "Installing "); write_str(1, pkgname);
        write_str(1, " -> "); write_str(1, destdir); write_str(1, "\n");

        int fd = open(pkg_path, O_RDONLY, 0);
        if (fd < 0) {
            write_str(2, "pkg_add: cannot open: "); write_str(2, pkg_path); write_str(2, "\n");
            status = 1; continue;
        }

        ExtractResult r = extract_tar(fd, destdir, verbose);
        close(fd);

        if (r.errors > 0) {
            write_str(2, "pkg_add: "); write_uint(2, static_cast<unsigned long>(r.errors));
            write_str(2, " error(s) during extraction of ");
            write_str(2, pkg_path); write_str(2, "\n");
            status = 1;
        }

        record_package(pkgname, r, pkg_path);

        write_str(1, "  ");
        write_uint(1, static_cast<unsigned long>(r.files));
        write_str(1, " files, ");
        write_uint(1, static_cast<unsigned long>(r.dirs));
        write_str(1, " dirs extracted\n");
    }

    return status;
}
