#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* Minimal sed: supports s/pat/rep/ and d commands, fixed-string matching */

struct Command {
    char type; /* 's' or 'd' */
    char pattern[128];
    char replacement[128];
    int global; /* g flag for s/// */
};

static struct Command g_cmds[8];
static int g_ncmds;
static int g_suppress; /* -n flag */

static int parse_s_command(const char *expr, struct Command *cmd) {
    if (*expr != 's' || !expr[1]) return 0;
    char delim = expr[1];
    const char *p = expr + 2;

    /* Extract pattern */
    int pi = 0;
    while (*p && *p != delim && pi < 127) cmd->pattern[pi++] = *p++;
    cmd->pattern[pi] = '\0';
    if (*p == delim) ++p;

    /* Extract replacement */
    int ri = 0;
    while (*p && *p != delim && ri < 127) cmd->replacement[ri++] = *p++;
    cmd->replacement[ri] = '\0';
    if (*p == delim) ++p;

    cmd->global = (*p == 'g');
    cmd->type = 's';
    return 1;
}

static void apply_substitute(char *line, struct Command *cmd) {
    char out[4096];
    size_t olen = 0;
    size_t plen = strlen(cmd->pattern);
    size_t rlen = strlen(cmd->replacement);
    char *p = line;

    if (plen == 0) { write(1, line, strlen(line)); write(1, "\n", 1); return; }

    while (*p) {
        if (strncmp(p, cmd->pattern, plen) == 0) {
            if (olen + rlen < sizeof(out)) {
                memcpy(out + olen, cmd->replacement, rlen);
                olen += rlen;
            }
            p += plen;
            if (!cmd->global) {
                /* Copy rest */
                size_t rest = strlen(p);
                if (olen + rest < sizeof(out)) {
                    memcpy(out + olen, p, rest);
                    olen += rest;
                }
                break;
            }
        } else {
            if (olen < sizeof(out) - 1) out[olen++] = *p;
            ++p;
        }
    }
    write(1, out, olen);
    write(1, "\n", 1);
}

static void process_line(char *line) {
    int deleted = 0;
    for (int i = 0; i < g_ncmds; ++i) {
        if (g_cmds[i].type == 'd') { deleted = 1; break; }
        if (g_cmds[i].type == 's') apply_substitute(line, &g_cmds[i]);
    }
    if (!deleted && g_ncmds == 0 && !g_suppress) {
        write(1, line, strlen(line));
        write(1, "\n", 1);
    }
}

int main(int argc, char **argv) {
    int argi = 1;
    while (argi < argc) {
        if (argv[argi][0] == '-' && argv[argi][1] == 'n') {
            g_suppress = 1;
            ++argi;
        } else if (argv[argi][0] == '-' && argv[argi][1] == 'e' && argi + 1 < argc) {
            ++argi;
            if (g_ncmds < 8) {
                if (argv[argi][0] == 'd') {
                    g_cmds[g_ncmds].type = 'd';
                    ++g_ncmds;
                } else {
                    if (parse_s_command(argv[argi], &g_cmds[g_ncmds])) ++g_ncmds;
                }
            }
            ++argi;
        } else if (argv[argi][0] != '-') {
            break;
        } else {
            /* Treat as expression */
            if (g_ncmds < 8) {
                if (argv[argi][0] == 'd') {
                    g_cmds[g_ncmds].type = 'd';
                    ++g_ncmds;
                } else {
                    if (parse_s_command(argv[argi], &g_cmds[g_ncmds])) ++g_ncmds;
                }
            }
            ++argi;
        }
    }

    int fd = 0;
    if (argi < argc) {
        fd = open(argv[argi], O_RDONLY, 0);
        if (fd < 0) { write(2, "sed: cannot open file\n", 22); return 1; }
    }

    char line[4096];
    ssize_t line_len = 0;
    char ch;
    while (read(fd, &ch, 1) == 1) {
        if (ch == '\n' || line_len >= (ssize_t)sizeof(line) - 1) {
            line[line_len] = '\0';
            process_line(line);
            line_len = 0;
        } else {
            line[line_len++] = ch;
        }
    }
    if (line_len > 0) {
        line[line_len] = '\0';
        process_line(line);
    }

    if (fd != 0) close(fd);
    return 0;
}
