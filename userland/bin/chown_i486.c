#include <unistd.h>
#include <string.h>

static long parse_num(const char *s) {
    long v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); ++s; }
    return v;
}

int main(int argc, char **argv) {
    if (argc < 3) { write(2, "usage: chown uid[:gid] file ...\n", 32); return 1; }
    long uid = 0, gid = -1;
    const char *spec = argv[1];
    uid = parse_num(spec);
    const char *colon = spec;
    while (*colon && *colon != ':') ++colon;
    if (*colon == ':') gid = parse_num(colon + 1);

    int status = 0;
    for (int i = 2; i < argc; ++i) {
        if (chown(argv[i], (unsigned)uid, (unsigned)(gid >= 0 ? gid : uid)) != 0) {
            write(2, "chown: failed for ", 18);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
            status = 1;
        }
    }
    return status;
}
