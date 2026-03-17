#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/* Minimal awk: pattern { print } with $0, $1..$NF, NR, NF, -F delim */

static char g_delim = ' ';
static char g_line[4096];
static char *g_fields[64];
static int g_nf;
static int g_nr;

static void split_fields(void) {
    g_nf = 0;
    char *p = g_line;
    while (*p && g_nf < 64) {
        while (*p == g_delim || (*p == '\t' && g_delim == ' ')) ++p;
        if (*p == '\0') break;
        g_fields[g_nf++] = p;
        if (g_delim == ' ') {
            while (*p && *p != ' ' && *p != '\t') ++p;
        } else {
            while (*p && *p != g_delim) ++p;
        }
        if (*p) *p++ = '\0';
    }
}

static void write_field(int n) {
    if (n == 0) { write(1, g_line, strlen(g_line)); return; }
    if (n >= 1 && n <= g_nf) write(1, g_fields[n-1], strlen(g_fields[n-1]));
}

static void write_num(long v) {
    char buf[24]; int pos = 0; int neg = 0;
    unsigned long uv;
    if (v < 0) { neg = 1; uv = (unsigned long)(-v); } else uv = (unsigned long)v;
    if (uv == 0) buf[pos++] = '0';
    else while (uv) { buf[pos++] = (char)('0' + uv % 10); uv /= 10; }
    if (neg) buf[pos++] = '-';
    for (int i = 0; i < pos/2; ++i) { char t=buf[i]; buf[i]=buf[pos-1-i]; buf[pos-1-i]=t; }
    write(1, buf, pos);
}

/* Simple action parser: print, print $N, print "str" */
static void execute_action(const char *action) {
    const char *p = action;
    while (*p == ' ' || *p == '\t') ++p;

    if (strncmp(p, "print", 5) == 0) {
        p += 5;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0' || *p == '}') {
            write(1, g_line, strlen(g_line));
            write(1, "\n", 1);
            return;
        }
        int first = 1;
        while (*p && *p != '}') {
            while (*p == ' ' || *p == '\t' || *p == ',') { if (*p == ',') first = 0; ++p; }
            if (!first) write(1, " ", 1);
            first = 0;
            if (*p == '$') {
                ++p;
                if (*p == 'N' && p[1] == 'F') { write_num(g_nf); p += 2; }
                else { int n = 0; while (*p >= '0' && *p <= '9') n = n*10 + (*p++ - '0'); write_field(n); }
            } else if (*p == 'N' && p[1] == 'R') { write_num(g_nr); p += 2; }
            else if (*p == 'N' && p[1] == 'F') { write_num(g_nf); p += 2; }
            else if (*p == '"') {
                ++p;
                while (*p && *p != '"') { write(1, p, 1); ++p; }
                if (*p == '"') ++p;
            } else break;
        }
        write(1, "\n", 1);
    }
}

int main(int argc, char **argv) {
    const char *program = 0;
    const char *input_file = 0;
    int argi = 1;

    while (argi < argc) {
        if (argv[argi][0] == '-' && argv[argi][1] == 'F' && argi + 1 < argc) {
            g_delim = argv[++argi][0];
            ++argi;
        } else if (program == 0) {
            program = argv[argi++];
        } else {
            input_file = argv[argi++];
        }
    }

    if (program == 0) { write(2, "usage: awk [-F c] 'program' [file]\n", 35); return 1; }

    /* Parse program: [/pattern/] { action } */
    const char *pattern = 0;
    const char *action = 0;
    const char *pp = program;
    while (*pp == ' ') ++pp;
    if (*pp == '/') {
        ++pp;
        pattern = pp;
        while (*pp && *pp != '/') ++pp;
        if (*pp == '/') ++pp;
    } else if (*pp == '{') {
        pattern = 0; /* match all lines */
    }
    while (*pp == ' ') ++pp;
    if (*pp == '{') {
        ++pp;
        action = pp;
    }

    int fd = 0;
    if (input_file) {
        fd = open(input_file, O_RDONLY, 0);
        if (fd < 0) { write(2, "awk: cannot open file\n", 22); return 1; }
    }

    ssize_t line_len = 0;
    char ch;
    while (read(fd, &ch, 1) == 1) {
        if (ch == '\n' || line_len >= (ssize_t)sizeof(g_line) - 1) {
            g_line[line_len] = '\0';
            ++g_nr;

            /* Save original line before splitting */
            char saved_line[4096];
            memcpy(saved_line, g_line, (size_t)line_len + 1);
            split_fields();
            memcpy(g_line, saved_line, (size_t)line_len + 1);

            /* Check pattern match */
            int match = 1;
            if (pattern) {
                size_t plen = 0;
                const char *pe = pattern;
                while (*pe && *pe != '/') { ++plen; ++pe; }
                match = (strstr(g_line, pattern) != 0);
                /* Crude: just check if pattern substring exists */
                /* For proper matching we'd need regex */
            }

            if (match && action) {
                execute_action(action);
            } else if (match && !action) {
                write(1, g_line, (size_t)line_len);
                write(1, "\n", 1);
            }
            line_len = 0;
        } else {
            g_line[line_len++] = ch;
        }
    }

    if (fd > 0) close(fd);
    return 0;
}
