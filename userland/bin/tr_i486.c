#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    int delete_mode = 0;
    int squeeze = 0;
    int argi = 1;

    while (argi < argc && argv[argi][0] == '-') {
        for (const char *p = argv[argi]+1; *p; ++p) {
            if (*p == 'd') delete_mode = 1;
            else if (*p == 's') squeeze = 1;
        }
        ++argi;
    }

    if (argi >= argc) {
        write(2, "usage: tr [-ds] set1 [set2]\n", 28);
        return 1;
    }

    const char *set1 = argv[argi++];
    const char *set2 = (argi < argc) ? argv[argi] : "";
    size_t len1 = strlen(set1);
    size_t len2 = strlen(set2);

    /* Build translation table */
    unsigned char map[256];
    unsigned char in_set1[256] = {0};
    for (int i = 0; i < 256; ++i) map[i] = (unsigned char)i;

    for (size_t i = 0; i < len1; ++i) {
        unsigned char c = (unsigned char)set1[i];
        in_set1[c] = 1;
        if (!delete_mode && len2 > 0) {
            unsigned char r = (unsigned char)(i < len2 ? set2[i] : set2[len2 - 1]);
            map[c] = r;
        }
    }

    char buf[512];
    ssize_t n;
    char last_out = '\0';

    while ((n = read(0, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; ++i) {
            unsigned char c = (unsigned char)buf[i];
            if (delete_mode && in_set1[c]) continue;
            char out = (char)map[c];
            if (squeeze && out == last_out && in_set1[c]) continue;
            write(1, &out, 1);
            last_out = out;
        }
    }
    return 0;
}
