// chown -- change file owner and group (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Stub: parses user[:group] numerically and calls chown().
// In the XINIM single-user model this mostly succeeds.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

void write_str(int fd, const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    while (n > 0) {
        auto w = write(fd, s, static_cast<unsigned>(n));
        if (w <= 0) return;
        s += w;
        n -= static_cast<int>(w);
    }
}

void err(const char* prog, const char* msg) {
    write_str(2, prog);
    write_str(2, ": ");
    write_str(2, msg);
    write_str(2, "\n");
}

void err2(const char* prog, const char* path, const char* msg) {
    write_str(2, prog);
    write_str(2, ": ");
    write_str(2, path);
    write_str(2, ": ");
    write_str(2, msg);
    write_str(2, "\n");
}

// Parse a non-negative decimal integer.  Returns -1 on invalid input.
int parse_uint(const char* s, int len) {
    if (len == 0) return -1;
    int val = 0;
    for (int i = 0; i < len; ++i) {
        if (s[i] < '0' || s[i] > '9') return -1;
        val = val * 10 + (s[i] - '0');
    }
    return val;
}

// Parse "user", "user:group", or ":group".
// Sets uid and gid; -1 means "do not change".
void parse_owner(const char* spec, int& uid, int& gid) {
    uid = -1;
    gid = -1;

    // Find colon separator
    int colon = -1;
    int len = 0;
    for (int i = 0; spec[i] != '\0'; ++i) {
        if (spec[i] == ':' && colon < 0) colon = i;
        ++len;
    }

    if (colon < 0) {
        // No colon: entire string is user
        uid = parse_uint(spec, len);
    } else {
        // Before colon is user (may be empty)
        if (colon > 0) uid = parse_uint(spec, colon);
        // After colon is group (may be empty)
        int glen = len - colon - 1;
        if (glen > 0) gid = parse_uint(spec + colon + 1, glen);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        err("chown", "usage: chown user[:group] file...");
        return 1;
    }

    int uid, gid;
    parse_owner(argv[1], uid, gid);

    if (uid < 0 && gid < 0) {
        err("chown", "invalid owner specification (use numeric uid[:gid])");
        return 1;
    }

    int status = 0;
    for (int i = 2; i < argc; ++i) {
        // Use -1 cast to uid_t/gid_t to mean "no change" per POSIX
        auto u = static_cast<unsigned>(uid);
        auto g = static_cast<unsigned>(gid);
        if (chown(argv[i], u, g) < 0) {
            err2("chown", argv[i], "cannot change owner");
            status = 1;
        }
    }

    return status;
}
