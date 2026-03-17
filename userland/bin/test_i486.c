#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

/* POSIX test / [ utility -- subset sufficient for shell scripts */

static int str_eq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int is_regular(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & 0170000) == 0100000;
}

static int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & 0170000) == 0040000;
}

static int is_nonzero(const char *s) {
    return s != 0 && s[0] != '\0';
}

static long to_long(const char *s) {
    long val = 0;
    int neg = 0;
    if (*s == '-') { neg = 1; ++s; }
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        ++s;
    }
    return neg ? -val : val;
}

static int eval(int argc, char **argv) {
    if (argc == 0) return 1; /* false */

    /* unary: -n STRING, -z STRING, -e FILE, -f FILE, -d FILE */
    if (argc == 2) {
        if (str_eq(argv[0], "-n")) return is_nonzero(argv[1]) ? 0 : 1;
        if (str_eq(argv[0], "-z")) return is_nonzero(argv[1]) ? 1 : 0;
        if (str_eq(argv[0], "-e")) return file_exists(argv[1]) ? 0 : 1;
        if (str_eq(argv[0], "-f")) return is_regular(argv[1]) ? 0 : 1;
        if (str_eq(argv[0], "-d")) return is_directory(argv[1]) ? 0 : 1;
        if (str_eq(argv[0], "-r")) return access(argv[1], 4) == 0 ? 0 : 1;
        if (str_eq(argv[0], "-w")) return access(argv[1], 2) == 0 ? 0 : 1;
        if (str_eq(argv[0], "-x")) return access(argv[1], 1) == 0 ? 0 : 1;
        if (str_eq(argv[0], "!")) return eval(argc - 1, argv + 1) ? 0 : 1;
        /* single arg: true if non-empty */
        return is_nonzero(argv[0]) ? 0 : 1;
    }

    /* single arg: true if non-empty */
    if (argc == 1) return is_nonzero(argv[0]) ? 0 : 1;

    /* binary: STRING = STRING, STRING != STRING, INT -eq INT, etc. */
    if (argc == 3) {
        if (str_eq(argv[1], "="))  return str_eq(argv[0], argv[2]) ? 0 : 1;
        if (str_eq(argv[1], "!=")) return str_eq(argv[0], argv[2]) ? 1 : 0;
        if (str_eq(argv[1], "-eq")) return to_long(argv[0]) == to_long(argv[2]) ? 0 : 1;
        if (str_eq(argv[1], "-ne")) return to_long(argv[0]) != to_long(argv[2]) ? 0 : 1;
        if (str_eq(argv[1], "-lt")) return to_long(argv[0]) <  to_long(argv[2]) ? 0 : 1;
        if (str_eq(argv[1], "-le")) return to_long(argv[0]) <= to_long(argv[2]) ? 0 : 1;
        if (str_eq(argv[1], "-gt")) return to_long(argv[0]) >  to_long(argv[2]) ? 0 : 1;
        if (str_eq(argv[1], "-ge")) return to_long(argv[0]) >= to_long(argv[2]) ? 0 : 1;
    }

    /* negation: ! EXPR */
    if (argc >= 2 && str_eq(argv[0], "!"))
        return eval(argc - 1, argv + 1) ? 0 : 1;

    /* unrecognized */
    return 2;
}

int main(int argc, char **argv) {
    int bracket = 0;
    const char *prog = argv[0];

    /* detect [ ... ] form */
    if (prog != 0) {
        size_t plen = strlen(prog);
        if (plen > 0 && prog[plen - 1] == '[') bracket = 1;
    }

    if (bracket) {
        if (argc < 2 || !str_eq(argv[argc - 1], "]")) {
            write(2, "[: missing ]\n", 13);
            return 2;
        }
        --argc; /* strip trailing ] */
    }

    return eval(argc - 1, argv + 1);
}
