#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* Minimal grep: fixed-string matching only (equivalent to fgrep) */

static int g_invert;
static int g_ignore_case;
static int g_count_only;
static int g_files_only;

static int casecmp(char a, char b) {
    if (a >= 'A' && a <= 'Z') a += 32;
    if (b >= 'A' && b <= 'Z') b += 32;
    return a == b;
}

static int match_fixed(const char *line, const char *pattern) {
    size_t plen = strlen(pattern);
    size_t llen = strlen(line);
    if (plen > llen) return 0;
    for (size_t i = 0; i <= llen - plen; ++i) {
        int found = 1;
        for (size_t j = 0; j < plen; ++j) {
            int eq = g_ignore_case ? casecmp(line[i+j], pattern[j])
                                   : (line[i+j] == pattern[j]);
            if (!eq) { found = 0; break; }
        }
        if (found) return 1;
    }
    return 0;
}

static int grep_fd(int fd, const char *pattern, const char *fname, int show_name) {
    char buf[4096];
    ssize_t buf_len = 0;
    ssize_t n;
    unsigned long match_count = 0;
    int status = 1;

    while ((n = read(fd, buf + buf_len, sizeof(buf) - 1 - buf_len)) > 0) {
        buf_len += n;
        buf[buf_len] = '\0';
        char *start = buf;
        char *nl;
        while ((nl = strchr(start, '\n')) != 0) {
            *nl = '\0';
            int matched = match_fixed(start, pattern);
            if (g_invert) matched = !matched;
            if (matched) {
                ++match_count;
                status = 0;
                if (!g_count_only && !g_files_only) {
                    if (show_name) { write(1, fname, strlen(fname)); write(1, ":", 1); }
                    write(1, start, strlen(start));
                    write(1, "\n", 1);
                }
                if (g_files_only) {
                    write(1, fname, strlen(fname));
                    write(1, "\n", 1);
                    return 0;
                }
            }
            start = nl + 1;
        }
        /* Move remaining partial line to start */
        buf_len = buf + buf_len - start;
        if (buf_len > 0) memmove(buf, start, buf_len);
    }
    /* Handle last line without newline */
    if (buf_len > 0) {
        buf[buf_len] = '\0';
        int matched = match_fixed(buf, pattern);
        if (g_invert) matched = !matched;
        if (matched) {
            ++match_count;
            status = 0;
            if (!g_count_only && !g_files_only) {
                if (show_name) { write(1, fname, strlen(fname)); write(1, ":", 1); }
                write(1, buf, buf_len);
                write(1, "\n", 1);
            }
        }
    }
    if (g_count_only) {
        char num[20];
        int pos = 0;
        unsigned long v = match_count;
        if (v == 0) num[pos++] = '0';
        else while (v > 0) { num[pos++] = (char)('0' + v % 10); v /= 10; }
        if (show_name) { write(1, fname, strlen(fname)); write(1, ":", 1); }
        for (int i = pos - 1; i >= 0; --i) write(1, num + i, 1);
        write(1, "\n", 1);
    }
    return status;
}

int main(int argc, char **argv) {
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] != '\0') {
        for (const char *p = argv[argi]+1; *p; ++p) {
            if (*p == 'i') g_ignore_case = 1;
            else if (*p == 'v') g_invert = 1;
            else if (*p == 'c') g_count_only = 1;
            else if (*p == 'l') g_files_only = 1;
        }
        ++argi;
    }

    if (argi >= argc) {
        write(2, "usage: grep [-ivcl] pattern [file ...]\n", 39);
        return 2;
    }

    const char *pattern = argv[argi++];
    if (argi >= argc) return grep_fd(0, pattern, "(stdin)", 0);

    int status = 1;
    int multi = (argc - argi > 1);
    for (int i = argi; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write(2, "grep: ", 6);
            write(2, argv[i], strlen(argv[i]));
            write(2, ": not found\n", 12);
            continue;
        }
        if (grep_fd(fd, pattern, argv[i], multi) == 0) status = 0;
        close(fd);
    }
    return status;
}
