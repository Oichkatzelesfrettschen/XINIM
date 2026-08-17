// tar -- tape archive utility (POSIX.1 ustar format)
// Cleanroom C++23 implementation per IEEE Std 1003.1 and POSIX ustar specification.
// Modes: -c (create), -x (extract), -t (list); modifiers: -v (verbose), -f (archive file).

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
    int n = 0; while (s[n]) ++n;
    write_all(fd, s, n);
}

int str_len(const char* s) noexcept { int n = 0; while (s[n]) ++n; return n; }

void str_copy(char* dst, const char* src, int cap) noexcept {
    int i = 0;
    while (i < cap - 1 && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

void mem_copy(void* dst, const void* src, int n) noexcept {
    auto* d = static_cast<char*>(dst);
    auto* s = static_cast<const char*>(src);
    for (int i = 0; i < n; ++i) d[i] = s[i];
}

void mem_zero(void* dst, int n) noexcept {
    auto* d = static_cast<char*>(dst); for (int i = 0; i < n; ++i) d[i] = '\0';
}

// POSIX ustar header (512 bytes)
struct UstarHeader {
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
static_assert(sizeof(UstarHeader) == 512, "ustar header must be exactly 512 bytes");

// Octal field helpers (per POSIX ustar spec section 10.1.1)
unsigned long read_octal(const char* s, int len) noexcept {
    unsigned long v = 0;
    for (int i = 0; i < len && s[i] >= '0' && s[i] <= '7'; ++i)
        v = v * 8U + static_cast<unsigned long>(s[i] - '0');
    return v;
}

void write_octal(char* dst, unsigned long v, int len) noexcept {
    // Fill len-1 digits then NUL; right-justified.
    mem_zero(dst, len);
    dst[len - 1] = '\0';
    for (int i = len - 2; i >= 0; --i) {
        dst[i] = static_cast<char>('0' + (v & 7U));
        v >>= 3;
    }
}

bool is_zero_block(const char* block) noexcept {
    for (int i = 0; i < 512; ++i) if (block[i] != '\0') return false;
    return true;
}

// Compute ustar checksum per POSIX: sum of bytes with checksum field treated as spaces.
unsigned compute_checksum(const UstarHeader* h) noexcept {
    const auto* b = reinterpret_cast<const unsigned char*>(h);
    unsigned sum = 0;
    for (int i = 0; i < 512; ++i) {
        if (i >= 148 && i < 156) sum += ' '; // checksum field treated as spaces
        else sum += b[i];
    }
    return sum;
}

// Create parent directories (per mkdir(2), ignoring EEXIST)
void make_parent_dirs(const char* path) noexcept {
    char buf[256]; int len = str_len(path);
    if (len >= 256) return;
    str_copy(buf, path, 256);
    for (int i = 1; i < len; ++i) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            mkdir(buf, 0755);
            buf[i] = '/';
        }
    }
}

// Build full path from ustar prefix + name fields.
bool build_path(const UstarHeader* h, char* out, int cap) noexcept {
    int pos = 0;
    if (h->prefix[0] != '\0') {
        int plen = str_len(h->prefix);
        if (plen >= cap - 2) return false;
        str_copy(out, h->prefix, cap);
        pos = plen;
        out[pos++] = '/';
    }
    int nlen = str_len(h->name);
    if (pos + nlen >= cap) return false;
    str_copy(out + pos, h->name, cap - pos);
    return true;
}

// ============================================================
// Extract mode
// ============================================================

int do_extract(int afd, bool verbose) noexcept {
    char block[512]; int zero_count = 0;

    while (read(afd, block, 512) == 512) {
        if (is_zero_block(block)) {
            if (++zero_count >= 2) break;
            continue;
        }
        zero_count = 0;

        auto* h = reinterpret_cast<UstarHeader*>(block);

        // Validate checksum
        unsigned stored = static_cast<unsigned>(read_octal(h->checksum, 8));
        unsigned computed = compute_checksum(h);
        if (stored != computed) {
            write_str(2, "tar: checksum mismatch, skipping block\n");
            continue;
        }

        unsigned long size = read_octal(h->size, 12);
        unsigned long mode = read_octal(h->mode, 8);

        char path[256];
        if (!build_path(h, path, sizeof(path))) {
            // Skip data blocks for oversized path
            unsigned long blks = (size + 511U) / 512U;
            for (unsigned long b = 0; b < blks; ++b) {
                char discard[512];
                static_cast<void>(read(afd, discard, 512));
            }
            continue;
        }

        if (verbose) { write_str(1, path); write_str(1, "\n"); }

        char typeflag = h->typeflag;
        if (typeflag == '\0') typeflag = '0'; // old GNU tar uses NUL for regular files

        if (typeflag == '5' || (typeflag == '0' && path[str_len(path) - 1] == '/')) {
            // Directory entry
            make_parent_dirs(path);
            mkdir(path, static_cast<mode_t>(mode & 0777U));
        } else if (typeflag == '0' || typeflag == '7') {
            // Regular file
            make_parent_dirs(path);
            int outfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, static_cast<int>(mode & 0666U));
            if (outfd < 0) {
                write_str(2, "tar: cannot create: "); write_str(2, path); write_str(2, "\n");
                // Skip data
                unsigned long blks = (size + 511U) / 512U;
                for (unsigned long b = 0; b < blks; ++b) {
                    char discard[512];
                    static_cast<void>(read(afd, discard, 512));
                }
                continue;
            }
            unsigned long remaining = size;
            while (remaining > 0) {
                char data[512];
                if (read(afd, data, 512) != 512) break;
                unsigned long chunk = remaining < 512U ? remaining : 512U;
                write_all(outfd, data, static_cast<int>(chunk));
                remaining -= chunk;
            }
            close(outfd);
        } else if (typeflag == '2') {
            // Symbolic link
            make_parent_dirs(path);
            // symlink(2) -- link to h->linkname
            // Per POSIX: just create the symlink; ignore if unsupported.
            // dietlibc should have symlink(2).
            symlink(h->linkname, path);
        } else if (typeflag == '1') {
            // Hard link
            make_parent_dirs(path);
            link(h->linkname, path);
        } else {
            // Skip other types (device nodes etc.)
            unsigned long blks = (size + 511U) / 512U;
            for (unsigned long b = 0; b < blks; ++b) {
                char discard[512];
                static_cast<void>(read(afd, discard, 512));
            }
        }
    }
    return 0;
}

// ============================================================
// List mode
// ============================================================

void write_uint(int fd, unsigned long v) noexcept {
    char tmp[24]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[24];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

int do_list(int afd, bool verbose) noexcept {
    char block[512]; int zero_count = 0;

    while (read(afd, block, 512) == 512) {
        if (is_zero_block(block)) {
            if (++zero_count >= 2) break;
            continue;
        }
        zero_count = 0;

        auto* h = reinterpret_cast<UstarHeader*>(block);
        unsigned long size = read_octal(h->size, 12);
        unsigned long mode = read_octal(h->mode, 8);

        char path[256];
        if (!build_path(h, path, sizeof(path))) {
            unsigned long blks = (size + 511U) / 512U;
            for (unsigned long b = 0; b < blks; ++b) {
                char discard[512];
                static_cast<void>(read(afd, discard, 512));
            }
            continue;
        }

        if (verbose) {
            // Mode string: -rwxrwxrwx or similar
            char mstr[11] = "----------";
            char tf = h->typeflag;
            if (tf == '5') mstr[0] = 'd';
            else if (tf == '2') mstr[0] = 'l';
            else if (tf == '1') mstr[0] = 'h';
            if (mode & 0400U) mstr[1] = 'r';
            if (mode & 0200U) mstr[2] = 'w';
            if (mode & 0100U) mstr[3] = 'x';
            if (mode & 0040U) mstr[4] = 'r';
            if (mode & 0020U) mstr[5] = 'w';
            if (mode & 0010U) mstr[6] = 'x';
            if (mode & 0004U) mstr[7] = 'r';
            if (mode & 0002U) mstr[8] = 'w';
            if (mode & 0001U) mstr[9] = 'x';
            write_all(1, mstr, 10);
            write_str(1, " ");
            write_uint(1, size);
            write_str(1, " ");
        }
        write_str(1, path);
        write_str(1, "\n");

        // Skip data blocks
        unsigned long blks = (size + 511U) / 512U;
        for (unsigned long b = 0; b < blks; ++b) {
            char discard[512];
            static_cast<void>(read(afd, discard, 512));
        }
    }
    return 0;
}

// ============================================================
// Create mode
// ============================================================

bool write_block(int afd, const char* data, int len) noexcept {
    // Pad to 512-byte block boundary
    char block[512]; mem_zero(block, 512);
    int to_copy = len < 512 ? len : 512;
    mem_copy(block, data, to_copy);
    return write(afd, block, 512) == 512;
}

bool write_header(int afd, const char* path, bool verbose, bool is_dir,
                  unsigned long size, unsigned long mode) noexcept {
    char block[512]; mem_zero(block, 512);
    auto* h = reinterpret_cast<UstarHeader*>(block);

    // Split long paths into prefix + name
    int plen = str_len(path);
    if (plen < 100) {
        str_copy(h->name, path, 100);
    } else {
        // Find split point: last '/' before 155/100 boundary
        int split = plen - 100;
        while (split > 0 && path[split] != '/') --split;
        if (split > 0 && split <= 155) {
            str_copy(h->prefix, path, split < 155 ? split + 1 : 155);
            h->prefix[split < 154 ? split : 154] = '\0';
            str_copy(h->name, path + split + 1, 100);
        } else {
            str_copy(h->name, path, 100); // truncate -- best effort
        }
    }

    write_octal(h->mode, mode, 8);
    write_octal(h->uid, 0, 8);
    write_octal(h->gid, 0, 8);
    write_octal(h->size, size, 12);
    write_octal(h->mtime, 0, 12);
    h->typeflag = is_dir ? '5' : '0';
    // Magic: "ustar" per POSIX.1-1988
    h->magic[0] = 'u'; h->magic[1] = 's'; h->magic[2] = 't';
    h->magic[3] = 'a'; h->magic[4] = 'r'; h->magic[5] = ' ';
    h->version[0] = ' '; h->version[1] = '\0';

    // Compute and write checksum
    unsigned csum = compute_checksum(h);
    write_octal(h->checksum, csum, 7);
    h->checksum[7] = ' '; // per convention

    if (verbose) { write_str(1, path); write_str(1, "\n"); }
    return write(afd, block, 512) == 512;
}

int create_file(int afd, const char* path, bool verbose) noexcept {
    struct stat st{};
    if (stat(path, &st) < 0) {
        write_str(2, "tar: cannot stat: "); write_str(2, path); write_str(2, "\n");
        return -1;
    }

    bool is_dir = (st.st_mode & 0170000U) == 0040000U;
    unsigned long size = is_dir ? 0UL : static_cast<unsigned long>(st.st_size);
    unsigned long mode = static_cast<unsigned long>(st.st_mode & 0777U);

    if (is_dir) {
        // Append trailing slash for directories
        char dpath[256]; str_copy(dpath, path, 255);
        int dlen = str_len(dpath);
        if (dlen < 254 && dpath[dlen - 1] != '/') { dpath[dlen] = '/'; dpath[dlen + 1] = '\0'; }
        write_header(afd, dpath, verbose, true, 0, mode);
    } else {
        if (!write_header(afd, path, verbose, false, size, mode)) return -1;
        int fd = open(path, O_RDONLY, 0);
        if (fd < 0) { write_str(2, "tar: cannot open: "); write_str(2, path); write_str(2, "\n"); return -1; }
        char buf[512]; unsigned long remaining = size;
        while (remaining > 0) {
            int to_read = remaining < 512U ? static_cast<int>(remaining) : 512;
            int nr = static_cast<int>(read(fd, buf, static_cast<unsigned>(to_read)));
            if (nr <= 0) break;
            // Pad rest of block with zeros
            for (int i = nr; i < 512; ++i) buf[i] = '\0';
            write_all(afd, buf, 512);
            remaining -= static_cast<unsigned long>(nr);
        }
        close(fd);
    }
    return 0;
}

// Write two zero blocks (end-of-archive marker per POSIX ustar spec)
void write_eof(int afd) noexcept {
    char block[512]; mem_zero(block, 512);
    write_all(afd, block, 512);
    write_all(afd, block, 512);
}

} // namespace

int main(int argc, char** argv) {
    enum class Mode { None, Extract, List, Create };
    Mode mode = Mode::None;
    bool verbose = false;
    const char* archive = nullptr;

    // Parse: tar [flags] [f archive] [files...]
    // Flags can be combined: tar xvf archive.tar or tar -xvf archive.tar
    int argi = 1;

    auto parse_flags = [&](const char* flags) {
        for (const char* p = flags; *p; ++p) {
            switch (*p) {
            case 'x': mode = Mode::Extract; break;
            case 't': mode = Mode::List;    break;
            case 'c': mode = Mode::Create;  break;
            case 'v': verbose = true;       break;
            case 'f':
                // next arg or immediate value
                if (p[1] != '\0') { archive = p + 1; return; } // -farchive
                if (argi < argc) archive = argv[argi++];
                return;
            default: break;
            }
        }
    };

    while (argi < argc) {
        const char* arg = argv[argi++];
        if (arg[0] == '-') {
            parse_flags(arg + 1);
        } else {
            // Flags without dash (legacy tar style)
            // Check if it looks like flags (contains x/t/c/v/f)
            bool has_mode = false;
            for (const char* p = arg; *p; ++p)
                if (*p == 'x' || *p == 't' || *p == 'c') { has_mode = true; break; }
            if (has_mode && mode == Mode::None) {
                parse_flags(arg);
            } else {
                // It's a file operand
                --argi; break;
            }
        }
    }

    if (mode == Mode::None) {
        write_str(2, "usage: tar {x|t|c}[vf] [archive] [files...]\n");
        write_str(2, "  tar xf archive.tar            -- extract\n");
        write_str(2, "  tar tf archive.tar            -- list\n");
        write_str(2, "  tar cf archive.tar file ...   -- create\n");
        return 1;
    }

    int afd = -1;
    bool close_afd = false;

    if (mode == Mode::Extract || mode == Mode::List) {
        if (archive == nullptr) {
            afd = 0; // stdin
        } else {
            afd = open(archive, O_RDONLY, 0);
            if (afd < 0) {
                write_str(2, "tar: cannot open: ");
                write_str(2, archive); write_str(2, "\n");
                return 1;
            }
            close_afd = true;
        }

        int rc = (mode == Mode::Extract) ? do_extract(afd, verbose) : do_list(afd, verbose);
        if (close_afd) close(afd);
        return rc;
    }

    // Create mode
    if (archive == nullptr) {
        afd = 1; // stdout
    } else {
        afd = open(archive, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (afd < 0) {
            write_str(2, "tar: cannot create: ");
            write_str(2, archive); write_str(2, "\n");
            return 1;
        }
        close_afd = true;
    }

    for (int i = argi; i < argc; ++i) {
        create_file(afd, argv[i], verbose);
    }

    write_eof(afd);
    if (close_afd) close(afd);
    return 0;
}
