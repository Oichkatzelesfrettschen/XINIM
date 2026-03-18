// sed -- stream editor (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Commands: s/pattern/replacement/[g], p, d, q
// Options: -n (suppress default print), -e EXPR (multiple expressions)
// Fixed-string pattern matching in s///.

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;
constexpr int kLineMax = 4096;
constexpr int kMaxCmds = 32;
constexpr int kPatMax = 256;

void write_all(int fd, const char* buf, int len) {
    while (len > 0) {
        auto w = write(fd, buf, static_cast<unsigned>(len));
        if (w <= 0) return;
        buf += w;
        len -= static_cast<int>(w);
    }
}

void write_str(int fd, const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    write_all(fd, s, n);
}

int str_len(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

enum CmdType : char {
    kSubstitute = 's',
    kPrint = 'p',
    kDelete = 'd',
    kQuit = 'q',
};

struct Command {
    CmdType type;
    char pattern[kPatMax];
    char replacement[kPatMax];
    bool global;       // g flag for s///
    int addr1;         // 0 = no address, >0 = line number
    int addr2;         // 0 = no range end
};

Command g_cmds[kMaxCmds];
int g_ncmds = 0;
bool g_suppress = false;

// Parse address prefix like "3" or "3,5" from expr, return pointer past it.
const char* parse_addr(const char* p, int* a1, int* a2) {
    *a1 = 0;
    *a2 = 0;
    if (*p >= '0' && *p <= '9') {
        while (*p >= '0' && *p <= '9') {
            *a1 = *a1 * 10 + (*p - '0');
            ++p;
        }
        if (*p == ',') {
            ++p;
            while (*p >= '0' && *p <= '9') {
                *a2 = *a2 * 10 + (*p - '0');
                ++p;
            }
        }
    }
    return p;
}

bool parse_command(const char* expr) {
    if (g_ncmds >= kMaxCmds) return false;
    auto& cmd = g_cmds[g_ncmds];
    cmd.global = false;
    cmd.addr1 = 0;
    cmd.addr2 = 0;

    const char* p = parse_addr(expr, &cmd.addr1, &cmd.addr2);

    if (*p == 's' && p[1] != '\0') {
        char delim = p[1];
        p += 2;

        // Extract pattern
        int pi = 0;
        while (*p != '\0' && *p != delim && pi < kPatMax - 1)
            cmd.pattern[pi++] = *p++;
        cmd.pattern[pi] = '\0';
        if (*p == delim) ++p;

        // Extract replacement
        int ri = 0;
        while (*p != '\0' && *p != delim && ri < kPatMax - 1) {
            cmd.replacement[ri++] = *p++;
        }
        cmd.replacement[ri] = '\0';
        if (*p == delim) ++p;

        // Flags
        while (*p != '\0') {
            if (*p == 'g') cmd.global = true;
            ++p;
        }
        cmd.type = kSubstitute;
        ++g_ncmds;
        return true;
    }

    if (*p == 'p') { cmd.type = kPrint; ++g_ncmds; return true; }
    if (*p == 'd') { cmd.type = kDelete; ++g_ncmds; return true; }
    if (*p == 'q') { cmd.type = kQuit; ++g_ncmds; return true; }

    return false;
}

// Apply s/pattern/replacement/[g] to line, write result to out.
// Returns true if any substitution was made.
bool apply_substitute(const char* line, int len, const Command& cmd,
                      char* out, int* out_len) {
    int plen = str_len(cmd.pattern);
    int rlen = str_len(cmd.replacement);
    int olen = 0;
    bool did_sub = false;

    if (plen == 0) {
        // Empty pattern: copy line unchanged
        for (int i = 0; i < len && olen < kLineMax - 1; ++i)
            out[olen++] = line[i];
        *out_len = olen;
        return false;
    }

    int i = 0;
    while (i < len) {
        if (i + plen <= len && memcmp(line + i, cmd.pattern, static_cast<unsigned>(plen)) == 0) {
            // Match found -- write replacement
            for (int r = 0; r < rlen && olen < kLineMax - 1; ++r) {
                if (cmd.replacement[r] == '&') {
                    // & in replacement means matched text
                    for (int m = 0; m < plen && olen < kLineMax - 1; ++m)
                        out[olen++] = line[i + m];
                } else {
                    out[olen++] = cmd.replacement[r];
                }
            }
            i += plen;
            did_sub = true;
            if (!cmd.global) {
                // Copy rest of line unchanged
                while (i < len && olen < kLineMax - 1)
                    out[olen++] = line[i++];
                break;
            }
        } else {
            if (olen < kLineMax - 1) out[olen++] = line[i];
            ++i;
        }
    }

    *out_len = olen;
    return did_sub;
}

bool addr_matches(const Command& cmd, int line_no) {
    if (cmd.addr1 == 0) return true;
    if (cmd.addr2 == 0) return line_no == cmd.addr1;
    return line_no >= cmd.addr1 && line_no <= cmd.addr2;
}

int process_stream(int fd) {
    char buf[kBufSize];
    char line[kLineMax];
    char work[kLineMax];
    int line_len = 0;
    int buf_pos = 0;
    int buf_end = 0;
    int line_no = 0;

    for (;;) {
        if (buf_pos >= buf_end) {
            auto n = read(fd, buf, sizeof(buf));
            if (n <= 0) {
                if (line_len > 0) goto process;
                break;
            }
            buf_pos = 0;
            buf_end = static_cast<int>(n);
        }

        {
            char ch = buf[buf_pos++];
            if (ch == '\n' || line_len >= kLineMax - 1) {
                goto process;
            }
            line[line_len++] = ch;
            continue;
        }

    process:
        line[line_len] = '\0';
        ++line_no;

        // Current working copy
        char* cur = line;
        int cur_len = line_len;
        bool deleted = false;
        bool printed = false;
        bool quit = false;

        for (int c = 0; c < g_ncmds; ++c) {
            if (!addr_matches(g_cmds[c], line_no)) continue;

            switch (g_cmds[c].type) {
            case kSubstitute: {
                int wlen = 0;
                bool did = apply_substitute(cur, cur_len, g_cmds[c], work, &wlen);
                if (did) {
                    // Copy work back to line for subsequent commands
                    memcpy(line, work, static_cast<unsigned>(wlen));
                    line[wlen] = '\0';
                    cur = line;
                    cur_len = wlen;
                }
                break;
            }
            case kPrint:
                write_all(1, cur, cur_len);
                write_all(1, "\n", 1);
                printed = true;
                break;
            case kDelete:
                deleted = true;
                break;
            case kQuit:
                if (!g_suppress) {
                    write_all(1, cur, cur_len);
                    write_all(1, "\n", 1);
                }
                return 0;
            }
            if (deleted) break;
        }

        if (!deleted && !g_suppress) {
            write_all(1, cur, cur_len);
            write_all(1, "\n", 1);
        }

        line_len = 0;

        // If we reached here from the end-of-file path, break
        if (buf_pos > buf_end) break;
        continue;
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    int first_file = 0;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'n' && argv[i][2] == '\0') {
            g_suppress = true;
        } else if (argv[i][0] == '-' && argv[i][1] == 'e') {
            // -e EXPR or -eEXPR
            const char* expr = nullptr;
            if (argv[i][2] != '\0') {
                expr = &argv[i][2];
            } else if (i + 1 < argc) {
                expr = argv[++i];
            }
            if (expr && !parse_command(expr)) {
                write_str(2, "sed: invalid command: ");
                write_str(2, expr);
                write_all(2, "\n", 1);
                return 1;
            }
        } else if (argv[i][0] != '-') {
            // First non-option: if no commands yet, treat as expression
            if (g_ncmds == 0) {
                if (!parse_command(argv[i])) {
                    write_str(2, "sed: invalid command: ");
                    write_str(2, argv[i]);
                    write_all(2, "\n", 1);
                    return 1;
                }
            } else {
                first_file = i;
                break;
            }
        } else {
            // Unknown option -- treat as expression
            if (g_ncmds == 0) {
                if (!parse_command(argv[i])) {
                    write_str(2, "sed: invalid command: ");
                    write_str(2, argv[i]);
                    write_all(2, "\n", 1);
                    return 1;
                }
            }
        }
    }

    if (g_ncmds == 0) {
        write_str(2, "usage: sed [-n] [-e cmd] [cmd] [file ...]\n");
        return 1;
    }

    if (first_file == 0) {
        // Find first non-option file arg after commands are parsed
        // Already consumed all args as commands/options; read stdin
        return process_stream(0);
    }

    int status = 0;
    for (int i = first_file; i < argc; ++i) {
        int fd;
        if (argv[i][0] == '-' && argv[i][1] == '\0') {
            fd = 0;
        } else {
            fd = open(argv[i], O_RDONLY, 0);
            if (fd < 0) {
                write_str(2, "sed: ");
                write_str(2, argv[i]);
                write_str(2, ": No such file or directory\n");
                status = 1;
                continue;
            }
        }
        if (process_stream(fd) != 0) status = 1;
        if (fd != 0) close(fd);
    }
    return status;
}
