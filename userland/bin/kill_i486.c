#include <unistd.h>
#include <string.h>
#include <signal.h>

/* Minimal signal name table */
static const struct { const char *name; int num; } g_sigs[] = {
    {"HUP", 1}, {"INT", 2}, {"QUIT", 3}, {"ILL", 4}, {"TRAP", 5},
    {"ABRT", 6}, {"BUS", 7}, {"FPE", 8}, {"KILL", 9}, {"USR1", 10},
    {"SEGV", 11}, {"USR2", 12}, {"PIPE", 13}, {"ALRM", 14}, {"TERM", 15},
    {"CHLD", 17}, {"CONT", 18}, {"STOP", 19}, {"TSTP", 20},
    {0, 0}
};

static long parse_long(const char *s) {
    long v = 0;
    int neg = 0;
    if (*s == '-') { neg = 1; ++s; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); ++s; }
    return neg ? -v : v;
}

static int lookup_signal(const char *name) {
    for (int i = 0; g_sigs[i].name; ++i) {
        if (strcmp(name, g_sigs[i].name) == 0) return g_sigs[i].num;
    }
    /* Try numeric */
    if (name[0] >= '0' && name[0] <= '9') return (int)parse_long(name);
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        write(2, "usage: kill [-signal] pid ...\n", 29);
        return 1;
    }

    int sig = 15; /* SIGTERM */
    int argi = 1;

    if (argv[1][0] == '-') {
        if (argv[1][1] == 'l') {
            for (int i = 0; g_sigs[i].name; ++i) {
                char num[4];
                int n = g_sigs[i].num;
                int pos = 0;
                if (n >= 10) num[pos++] = (char)('0' + n / 10);
                num[pos++] = (char)('0' + n % 10);
                num[pos++] = ')';
                write(1, num, pos);
                write(1, " ", 1);
                write(1, g_sigs[i].name, strlen(g_sigs[i].name));
                write(1, "\n", 1);
            }
            return 0;
        }
        sig = lookup_signal(argv[1] + 1);
        if (sig < 0) {
            write(2, "kill: unknown signal\n", 21);
            return 1;
        }
        argi = 2;
    }

    int status = 0;
    for (int i = argi; i < argc; ++i) {
        int pid = (int)parse_long(argv[i]);
        if (kill(pid, sig) != 0) {
            write(2, "kill: failed for pid ", 21);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
            status = 1;
        }
    }
    return status;
}
