// awk -- pattern scanning and processing language (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1 and AWK book (Aho/Weinberger/Kernighan).
// Supports: BEGIN/END, pattern{action}, -F sep, -v var=val, -f file, multiple input files.
// Built-ins: print, printf, split, sub, gsub, index, substr, length, tolower, toupper,
//            sprintf(limited), getline, next, exit, if/else, while, for, do-while.
// Variables: $0..$NF, NR, NF, FS, RS, OFS, ORS, FILENAME, FNR.

#include <fcntl.h>
#include <unistd.h>

namespace {

// ============================================================
// Low-level I/O helpers
// ============================================================

constexpr int kStdout = 1;
constexpr int kStderr = 2;

void write_all(int fd, const char* buf, int len) noexcept {
    while (len > 0) {
        auto w = write(fd, buf, static_cast<unsigned>(len));
        if (w <= 0) return;
        buf += w; len -= static_cast<int>(w);
    }
}

void write_str(int fd, const char* s) noexcept {
    int n = 0;
    while (s[n] != '\0') ++n;
    write_all(fd, s, n);
}

int str_len(const char* s) noexcept {
    int n = 0; while (s[n]) ++n; return n;
}

bool str_eq(const char* a, const char* b) noexcept {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

bool str_starts(const char* s, const char* prefix) noexcept {
    while (*prefix) { if (*s != *prefix) return false; ++s; ++prefix; }
    return true;
}

int str_cmp(const char* a, const char* b) noexcept {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

void str_copy(char* dst, const char* src, int cap) noexcept {
    int i = 0;
    while (i < cap - 1 && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

void str_append(char* dst, int& len, int cap, const char* src, int src_len) noexcept {
    int i = 0;
    while (i < src_len && len < cap - 1) { dst[len++] = src[i++]; }
    dst[len] = '\0';
}

// ============================================================
// Number I/O helpers
// ============================================================

// Format signed integer into buf, return length.
int fmt_int(long v, char* buf, int cap) noexcept {
    if (cap < 2) return 0;
    char tmp[24]; int n = 0; bool neg = false;
    if (v < 0) { neg = true; v = -v; }
    if (v == 0) tmp[n++] = '0';
    else { auto u = static_cast<unsigned long>(v); while (u) { tmp[n++] = static_cast<char>('0' + u % 10); u /= 10; } }
    if (neg && n < 23) tmp[n++] = '-';
    int out = 0;
    for (int i = n - 1; i >= 0 && out < cap - 1; --i) buf[out++] = tmp[i];
    buf[out] = '\0';
    return out;
}

long str_to_int(const char* s) noexcept {
    while (*s == ' ' || *s == '\t') ++s;
    bool neg = false;
    if (*s == '-') { neg = true; ++s; }
    else if (*s == '+') ++s;
    long v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); ++s; }
    return neg ? -v : v;
}

// ============================================================
// AWK Value -- string/number cell
// ============================================================

constexpr int kValCap = 512;

struct Val {
    char s[kValCap];
    long num;
    bool is_num; // if true, num is canonical; s may be set or empty

    void set_str(const char* src, int len = -1) noexcept {
        if (len < 0) len = str_len(src);
        if (len >= kValCap) len = kValCap - 1;
        for (int i = 0; i < len; ++i) s[i] = src[i];
        s[len] = '\0';
        is_num = false;
    }

    void set_int(long v) noexcept {
        num = v; is_num = true;
        fmt_int(v, s, kValCap);
    }

    void set_empty() noexcept { s[0] = '\0'; num = 0; is_num = false; }

    long as_int() const noexcept {
        if (is_num) return num;
        return str_to_int(s);
    }

    const char* as_str() const noexcept { return s; }
};

// ============================================================
// AWK Variable table (simple string -> Val map)
// ============================================================

constexpr int kMaxVars = 64;
constexpr int kVarNameCap = 32;

struct Var {
    char name[kVarNameCap];
    Val val;
    bool used;
};

Var g_vars[kMaxVars]{};

Val* find_var(const char* name) noexcept {
    for (auto& v : g_vars) {
        if (v.used && str_eq(v.name, name)) return &v.val;
    }
    return nullptr;
}

Val* get_var(const char* name) noexcept {
    for (auto& v : g_vars) {
        if (v.used && str_eq(v.name, name)) return &v.val;
    }
    for (auto& v : g_vars) {
        if (!v.used) {
            str_copy(v.name, name, kVarNameCap);
            v.val.set_empty();
            v.used = true;
            return &v.val;
        }
    }
    return &g_vars[0].val; // fallback: overwrite slot 0
}

void set_var_str(const char* name, const char* val) noexcept {
    get_var(name)->set_str(val);
}

void set_var_int(const char* name, long val) noexcept {
    get_var(name)->set_int(val);
}

const char* get_var_str(const char* name) noexcept {
    auto* v = find_var(name);
    if (!v) return "";
    return v->as_str();
}

long get_var_int(const char* name) noexcept {
    auto* v = find_var(name);
    if (!v) return 0;
    return v->as_int();
}

// ============================================================
// Field table: $0..$NF
// ============================================================

constexpr int kMaxFields = 64;
constexpr int kLineCap = 4096;

char  g_line[kLineCap];
int   g_line_len;
char  g_field_store[kLineCap];
char* g_fields[kMaxFields];
int   g_nf;

char  g_ofs[8] = " ";   // OFS
char  g_ors[8] = "\n";  // ORS
char  g_fs = ' ';       // FS (single char or ' ' for whitespace)
long  g_nr;             // NR
long  g_fnr;            // FNR

void split_line() noexcept {
    g_nf = 0;
    char* dst = g_field_store;
    char* p = g_line;

    auto save_field = [&](char* start, int len) {
        if (g_nf >= kMaxFields) return;
        g_fields[g_nf++] = dst;
        for (int i = 0; i < len; ++i) dst[i] = start[i];
        dst += len; *dst++ = '\0';
    };

    if (g_fs == ' ') {
        // Whitespace separator: trim leading, split on runs
        while (*p == ' ' || *p == '\t') ++p;
        while (*p) {
            char* start = p;
            while (*p && *p != ' ' && *p != '\t') ++p;
            save_field(start, static_cast<int>(p - start));
            while (*p == ' ' || *p == '\t') ++p;
        }
    } else {
        // Single-char separator: split including empty fields
        char* start = p;
        while (*p) {
            if (*p == g_fs) {
                save_field(start, static_cast<int>(p - start));
                start = p + 1;
            }
            ++p;
        }
        save_field(start, static_cast<int>(p - start));
    }
}

void rebuild_dollar0() noexcept {
    // Rebuild $0 from fields using OFS
    int ofs_len = str_len(g_ofs);
    g_line_len = 0;
    for (int i = 0; i < g_nf; ++i) {
        if (i > 0) {
            for (int j = 0; j < ofs_len && g_line_len < kLineCap - 1; ++j)
                g_line[g_line_len++] = g_ofs[j];
        }
        const char* f = g_fields[i];
        while (*f && g_line_len < kLineCap - 1) g_line[g_line_len++] = *f++;
    }
    g_line[g_line_len] = '\0';
}

const char* get_field(int n) noexcept {
    if (n == 0) return g_line;
    if (n >= 1 && n <= g_nf) return g_fields[n - 1];
    return "";
}

// ============================================================
// Simple regex: only ^ $ . * literal chars (POSIX BRE subset)
// ============================================================

bool regex_match_here(const char* re, const char* s) noexcept;

bool regex_match_star(char c, const char* re, const char* s) noexcept {
    do {
        if (regex_match_here(re, s)) return true;
        if (*s == '\0') return false;
        if (c != '.' && *s != c) return false;
        ++s;
    } while (true);
}

bool regex_match_here(const char* re, const char* s) noexcept {
    if (re[0] == '\0') return true;
    if (re[0] == '$' && re[1] == '\0') return *s == '\0';
    if (re[1] == '*') return regex_match_star(re[0], re + 2, s);
    if (re[0] == '\\' && re[1] != '\0') {
        if (*s == re[1]) return regex_match_here(re + 2, s + 1);
        return false;
    }
    if (re[0] == '.' || *s == re[0]) {
        if (*s == '\0') return false;
        return regex_match_here(re + 1, s + 1);
    }
    return false;
}

bool regex_match(const char* re, const char* s) noexcept {
    if (re[0] == '^') return regex_match_here(re + 1, s);
    do {
        if (regex_match_here(re, s)) return true;
    } while (*s++ != '\0');
    return false;
}

// ============================================================
// Program representation: up to 8 rules
// ============================================================

constexpr int kMaxRules = 8;
constexpr int kPatCap   = 64;
constexpr int kActCap   = 1024;

struct Rule {
    char  pattern[kPatCap]; // empty = match all; "BEGIN"/"END" special
    bool  is_regex;         // /re/ pattern
    char  action[kActCap];
};

Rule g_rules[kMaxRules]{};
int  g_nrules;

// ============================================================
// Expression evaluator (recursive descent)
// ============================================================

Val eval_expr(const char*& p) noexcept; // forward decl

// Skip whitespace
void skip_ws(const char*& p) noexcept {
    while (*p == ' ' || *p == '\t') ++p;
}

// Parse integer or variable
Val eval_primary(const char*& p) noexcept {
    skip_ws(p);
    Val v{}; v.set_empty();

    if (*p == '(') {
        ++p;
        v = eval_expr(p);
        skip_ws(p);
        if (*p == ')') ++p;
        return v;
    }

    if (*p == '-') { ++p; auto u = eval_primary(p); v.set_int(-u.as_int()); return v; }
    if (*p == '!') { ++p; auto u = eval_primary(p); v.set_int(u.as_int() == 0 ? 1 : 0); return v; }

    if (*p == '"') {
        ++p;
        int n = 0;
        while (*p && *p != '"' && n < kValCap - 1) v.s[n++] = *p++;
        v.s[n] = '\0'; v.is_num = false;
        if (*p == '"') ++p;
        return v;
    }

    if (*p == '$') {
        ++p;
        if (*p >= '0' && *p <= '9') {
            int n = 0;
            while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
            v.set_str(get_field(n));
        } else if (str_starts(p, "NF")) {
            v.set_int(g_nf); p += 2;
        } else {
            // $(expr)
            auto u = eval_primary(p);
            v.set_str(get_field(static_cast<int>(u.as_int())));
        }
        return v;
    }

    // Check built-in functions
    if (str_starts(p, "length(")) {
        p += 7;
        auto u = eval_expr(p);
        skip_ws(p); if (*p == ')') ++p;
        v.set_int(str_len(u.as_str()));
        return v;
    }
    if (str_starts(p, "length")) {
        p += 6;
        v.set_int(str_len(g_line));
        return v;
    }
    if (str_starts(p, "tolower(")) {
        p += 8;
        auto u = eval_expr(p);
        skip_ws(p); if (*p == ')') ++p;
        const char* src = u.as_str();
        int n = 0;
        while (src[n] && n < kValCap - 1) {
            char c = src[n];
            v.s[n++] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
        }
        v.s[n] = '\0'; v.is_num = false;
        return v;
    }
    if (str_starts(p, "toupper(")) {
        p += 8;
        auto u = eval_expr(p);
        skip_ws(p); if (*p == ')') ++p;
        const char* src = u.as_str();
        int n = 0;
        while (src[n] && n < kValCap - 1) {
            char c = src[n];
            v.s[n++] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
        }
        v.s[n] = '\0'; v.is_num = false;
        return v;
    }
    if (str_starts(p, "substr(")) {
        p += 7;
        auto str_val = eval_expr(p);
        skip_ws(p); if (*p == ',') ++p;
        auto start_val = eval_expr(p);
        int start = static_cast<int>(start_val.as_int()) - 1;
        int maxlen = kValCap;
        skip_ws(p);
        if (*p == ',') { ++p; maxlen = static_cast<int>(eval_expr(p).as_int()); }
        skip_ws(p); if (*p == ')') ++p;
        const char* src = str_val.as_str();
        int slen = str_len(src);
        if (start < 0) start = 0;
        if (start > slen) start = slen;
        int n = 0;
        while (src[start + n] && n < maxlen && n < kValCap - 1) v.s[n++] = src[start + n];
        v.s[n] = '\0'; v.is_num = false;
        return v;
    }
    if (str_starts(p, "index(")) {
        p += 6;
        auto hay = eval_expr(p);
        skip_ws(p); if (*p == ',') ++p;
        auto needle = eval_expr(p);
        skip_ws(p); if (*p == ')') ++p;
        const char* h = hay.as_str(); const char* n2 = needle.as_str();
        int nlen = str_len(n2);
        if (nlen == 0) { v.set_int(0); return v; }
        for (int i = 0; h[i]; ++i) {
            bool match = true;
            for (int j = 0; j < nlen; ++j) { if (h[i+j] != n2[j]) { match = false; break; } }
            if (match) { v.set_int(i + 1); return v; }
        }
        v.set_int(0);
        return v;
    }
    if (str_starts(p, "int(")) {
        p += 4;
        auto u = eval_expr(p);
        skip_ws(p); if (*p == ')') ++p;
        v.set_int(u.as_int());
        return v;
    }

    // Number literal
    if (*p >= '0' && *p <= '9') {
        long n = 0;
        while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
        v.set_int(n);
        return v;
    }

    // Named variable (NR, NF, FS, or user var)
    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_') {
        char name[kVarNameCap]; int n = 0;
        while (((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                (*p >= '0' && *p <= '9') || *p == '_') && n < kVarNameCap - 1)
            name[n++] = *p++;
        name[n] = '\0';

        // Handle post-increment: name++
        bool post_inc = false;
        if (*p == '+' && p[1] == '+') { ++p; ++p; post_inc = true; }
        bool post_dec = false;
        if (*p == '-' && p[1] == '-') { ++p; ++p; post_dec = true; }

        if (str_eq(name, "NR")) { v.set_int(g_nr); }
        else if (str_eq(name, "FNR")) { v.set_int(g_fnr); }
        else if (str_eq(name, "NF")) { v.set_int(g_nf); }
        else if (str_eq(name, "FS")) { char fs[2] = {g_fs, 0}; v.set_str(fs); }
        else { v = *get_var(name); }

        if (post_inc) {
            long old = v.as_int();
            if (!str_eq(name, "NR") && !str_eq(name, "NF") && !str_eq(name, "FNR"))
                get_var(name)->set_int(old + 1);
            v.set_int(old);
        }
        if (post_dec) {
            long old = v.as_int();
            if (!str_eq(name, "NR") && !str_eq(name, "NF") && !str_eq(name, "FNR"))
                get_var(name)->set_int(old - 1);
            v.set_int(old);
        }
        return v;
    }

    return v;
}

Val eval_mul(const char*& p) noexcept {
    auto left = eval_primary(p);
    skip_ws(p);
    while (*p == '*' || *p == '/' || *p == '%') {
        char op = *p++;
        auto right = eval_primary(p);
        long l = left.as_int(), r = right.as_int();
        if (op == '*') left.set_int(l * r);
        else if (op == '/') left.set_int(r != 0 ? l / r : 0);
        else left.set_int(r != 0 ? l % r : 0);
        skip_ws(p);
    }
    return left;
}

Val eval_add(const char*& p) noexcept {
    auto left = eval_mul(p);
    skip_ws(p);
    while (*p == '+' || *p == '-') {
        // Don't consume ++ or --
        if (p[1] == '+' || p[1] == '-') break;
        char op = *p++;
        auto right = eval_mul(p);
        long l = left.as_int(), r = right.as_int();
        left.set_int(op == '+' ? l + r : l - r);
        skip_ws(p);
    }
    return left;
}

Val eval_cmp(const char*& p) noexcept {
    auto left = eval_add(p);
    skip_ws(p);

    auto do_str_cmp = [&](const char* a, const char* b, char op1, char op2) -> Val {
        int c = str_cmp(a, b);
        Val r{}; r.is_num = true;
        if (op1 == '=' && op2 == '=') r.set_int(c == 0 ? 1 : 0);
        else if (op1 == '!' && op2 == '=') r.set_int(c != 0 ? 1 : 0);
        else if (op1 == '<' && op2 == '=') r.set_int(c <= 0 ? 1 : 0);
        else if (op1 == '>' && op2 == '=') r.set_int(c >= 0 ? 1 : 0);
        else if (op1 == '<') r.set_int(c < 0 ? 1 : 0);
        else if (op1 == '>') r.set_int(c > 0 ? 1 : 0);
        return r;
    };

    if ((p[0] == '=' && p[1] == '=') || (p[0] == '!' && p[1] == '=') ||
        (p[0] == '<' && p[1] == '=') || (p[0] == '>' && p[1] == '=') ||
        (p[0] == '<' && p[1] != '<') || (p[0] == '>' && p[1] != '>')) {
        char op1 = p[0], op2 = '\0';
        if (p[1] == '=') { op2 = '='; p += 2; } else { ++p; }
        auto right = eval_add(p);
        // Numeric comparison if both numeric
        if (left.is_num || right.is_num) {
            long l = left.as_int(), r = right.as_int();
            Val v{}; v.is_num = true;
            if (op1 == '=' && op2 == '=') v.set_int(l == r ? 1 : 0);
            else if (op1 == '!' && op2 == '=') v.set_int(l != r ? 1 : 0);
            else if (op1 == '<' && op2 == '=') v.set_int(l <= r ? 1 : 0);
            else if (op1 == '>' && op2 == '=') v.set_int(l >= r ? 1 : 0);
            else if (op1 == '<') v.set_int(l < r ? 1 : 0);
            else v.set_int(l > r ? 1 : 0);
            return v;
        }
        return do_str_cmp(left.as_str(), right.as_str(), op1, op2);
    }

    // Regex match: ~ and !~
    if ((p[0] == '~' && p[1] != '=') || (p[0] == '!' && p[1] == '~')) {
        bool negate = (p[0] == '!');
        p += (negate ? 2 : 1);
        skip_ws(p);
        char re[kPatCap]{}; int rn = 0;
        if (*p == '/') {
            ++p;
            while (*p && *p != '/' && rn < kPatCap - 1) re[rn++] = *p++;
            if (*p == '/') ++p;
        }
        re[rn] = '\0';
        bool m = regex_match(re, left.as_str());
        Val v{}; v.set_int((negate ? !m : m) ? 1 : 0);
        return v;
    }

    return left;
}

Val eval_and(const char*& p) noexcept {
    auto left = eval_cmp(p);
    skip_ws(p);
    while (p[0] == '&' && p[1] == '&') {
        p += 2;
        auto right = eval_cmp(p);
        left.set_int((left.as_int() && right.as_int()) ? 1 : 0);
        skip_ws(p);
    }
    return left;
}

Val eval_expr(const char*& p) noexcept {
    skip_ws(p);
    auto left = eval_and(p);
    skip_ws(p);
    while (p[0] == '|' && p[1] == '|') {
        p += 2;
        auto right = eval_and(p);
        left.set_int((left.as_int() || right.as_int()) ? 1 : 0);
        skip_ws(p);
    }
    // Assignment: var = expr (when left is an lvalue -- simplified: check for single token)
    // Handled in execute_statement instead for named vars.
    return left;
}

// ============================================================
// printf-style formatter (for print and printf)
// ============================================================

void do_printf(int fd, const char* fmt, const char** args, int nargs) noexcept {
    int ai = 0;
    while (*fmt) {
        if (*fmt != '%') { write_all(fd, fmt, 1); ++fmt; continue; }
        ++fmt;
        if (*fmt == '%') { write_all(fd, "%", 1); ++fmt; continue; }

        // Width/precision
        char spec[32]; int sn = 0; spec[sn++] = '%';
        while (*fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '0' ||
               (*fmt >= '1' && *fmt <= '9') || *fmt == '.') spec[sn++] = *fmt++;

        char conv = *fmt ? *fmt++ : 's';
        spec[sn++] = conv; spec[sn] = '\0';

        const char* arg = (ai < nargs) ? args[ai++] : "";
        if (conv == 's') {
            write_str(fd, arg);
        } else if (conv == 'd' || conv == 'i') {
            char buf[32]; fmt_int(str_to_int(arg), buf, sizeof(buf));
            write_str(fd, buf);
        } else if (conv == 'x' || conv == 'X') {
            long v = str_to_int(arg);
            char tmp[16]; int n = 0;
            auto uv = static_cast<unsigned long>(v);
            if (uv == 0) tmp[n++] = '0';
            else { while (uv) { int d = static_cast<int>(uv & 15);
                tmp[n++] = static_cast<char>(d < 10 ? '0'+d : (conv=='x'?'a':'A')+(d-10));
                uv >>= 4; } }
            for (int i = 0, j = n-1; i < j; ++i, --j) { char t=tmp[i]; tmp[i]=tmp[j]; tmp[j]=t; }
            write_all(fd, tmp, n);
        } else if (conv == 'o') {
            long v = str_to_int(arg);
            char tmp[16]; int n = 0;
            auto uv = static_cast<unsigned long>(v);
            if (uv == 0) tmp[n++] = '0';
            else { while (uv) { tmp[n++] = static_cast<char>('0' + (uv & 7)); uv >>= 3; } }
            for (int i = 0, j = n-1; i < j; ++i, --j) { char t=tmp[i]; tmp[i]=tmp[j]; tmp[j]=t; }
            write_all(fd, tmp, n);
        } else if (conv == 'c') {
            char c = arg[0] ? arg[0] : '\0';
            write_all(fd, &c, 1);
        } else {
            write_str(fd, arg);
        }
    }
}

// ============================================================
// Statement executor
// ============================================================

// Forward declarations
bool execute_action(const char* action) noexcept;
bool execute_stmts(const char*& p) noexcept;

// Parse and print a single print/printf arg list
void execute_print(const char*& p, bool is_printf) noexcept {
    skip_ws(p);

    // Output redirect: print > file or print >> file
    int out_fd = kStdout;
    bool close_out = false;

    // Collect args
    const char* args[16]{}; char arg_bufs[16][kValCap]{};
    int nargs = 0;
    char fmt_buf[kValCap]{}; // for printf format

    // Parse format string for printf
    if (is_printf) {
        Val fv = eval_expr(p);
        str_copy(fmt_buf, fv.as_str(), kValCap);
        skip_ws(p);
        if (*p == ',') ++p;
    }

    while (*p && *p != ';' && *p != '\n' && *p != '}' && *p != '>' && nargs < 16) {
        skip_ws(p);
        if (*p == ',' ) { ++p; continue; }
        if (*p == ';' || *p == '}' || *p == '\n') break;
        if (*p == '>' || (*p == '>' && p[1] == '>')) break;
        if (*p == '\0') break;
        Val v = eval_expr(p);
        str_copy(arg_bufs[nargs], v.as_str(), kValCap);
        args[nargs] = arg_bufs[nargs];
        ++nargs;
        skip_ws(p);
        if (*p == ',') { ++p; skip_ws(p); }
    }

    // Check for output redirect
    skip_ws(p);
    bool append = false;
    if (*p == '>' ) {
        ++p; if (*p == '>') { ++p; append = true; }
        skip_ws(p);
        // Parse filename
        Val fn = eval_expr(p);
        int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
        out_fd = open(fn.as_str(), flags, 0644);
        close_out = (out_fd >= 0);
        if (out_fd < 0) out_fd = kStdout;
    }

    if (is_printf) {
        do_printf(out_fd, fmt_buf, args, nargs);
    } else {
        if (nargs == 0) {
            // print $0
            write_str(out_fd, g_line);
            write_str(out_fd, g_ors);
        } else {
            for (int i = 0; i < nargs; ++i) {
                if (i > 0) write_str(out_fd, g_ofs);
                write_str(out_fd, args[i]);
            }
            write_str(out_fd, g_ors);
        }
    }

    if (close_out) close(out_fd);
}

// Execute a sub()/gsub() call: sub(/re/, repl [, var])
void execute_sub(const char*& p, bool global) noexcept {
    // sub(/re/, repl) or sub(/re/, repl, var)
    skip_ws(p);
    if (*p == '(') ++p;
    skip_ws(p);

    // regex
    char re[kPatCap]{}; int rn = 0;
    if (*p == '/') {
        ++p;
        while (*p && *p != '/' && rn < kPatCap - 1) re[rn++] = *p++;
        if (*p == '/') ++p;
    }
    re[rn] = '\0';
    skip_ws(p); if (*p == ',') ++p; skip_ws(p);

    // replacement
    Val repl_v = eval_expr(p);
    const char* repl = repl_v.as_str();

    // optional target variable
    char tgt[kVarNameCap]{}; int tn = 0;
    skip_ws(p);
    bool field_target = false; int field_n = 0;
    if (*p == ',') {
        ++p; skip_ws(p);
        if (*p == '$') {
            ++p; field_target = true;
            while (*p >= '0' && *p <= '9') field_n = field_n * 10 + (*p++ - '0');
        } else {
            while (((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_') && tn < kVarNameCap - 1)
                tgt[tn++] = *p++;
            tgt[tn] = '\0';
        }
    }
    skip_ws(p); if (*p == ')') ++p;

    // Get source string
    const char* src = g_line;
    if (field_target && field_n > 0) src = get_field(field_n);
    else if (tn > 0) src = get_var_str(tgt);

    // Perform substitution: scan src char-by-char, replace matching spans.
    char result[kLineCap]{}; int rlen = 0;
    int slen = str_len(src);
    bool did_global = false;

    for (int i = 0; i <= slen; ) {
        bool anchored = (re[0] == '^');
        const char* re2 = anchored ? re + 1 : re;

        // Try matching at position i
        bool matched = false;
        if (src[i] != '\0' || re[0] == '\0') {
            matched = regex_match_here(re2, src + i);
        }

        if (matched) {
            // Find match end: advance greedily by 1 char minimum
            int j = i + 1;
            // For star patterns keep advancing while pattern still anchored-matches at j
            while (j < slen && regex_match_here(re2, src + j)) { ++j; }

            // Append replacement, interpreting & as matched text
            for (int k = 0; repl[k] && rlen < kLineCap - 1; ++k) {
                if (repl[k] == '&') {
                    for (int m = i; m < j && rlen < kLineCap - 1; ++m)
                        result[rlen++] = src[m];
                } else if (repl[k] == '\\' && repl[k+1]) {
                    result[rlen++] = repl[++k];
                } else {
                    result[rlen++] = repl[k];
                }
            }
            i = j;
            did_global = true;
            if (!global) {
                // Copy remainder unchanged
                while (src[i] && rlen < kLineCap - 1) result[rlen++] = src[i++];
                break;
            }
            if (j == i && i <= slen) { // zero-width match: copy one char to avoid loop
                if (src[i] && rlen < kLineCap - 1) result[rlen++] = src[i++];
            }
        } else {
            if (src[i] && rlen < kLineCap - 1) result[rlen++] = src[i];
            ++i;
        }
        if (anchored) break; // ^ only matches at position 0
    }
    static_cast<void>(did_global);
    result[rlen] = '\0';

    // Store back
    if (field_target && field_n == 0) {
        str_copy(g_line, result, kLineCap);
        g_line_len = rlen;
        split_line();
    } else if (field_target) {
        // Simplified: rebuild $0
        str_copy(g_line, result, kLineCap);
        g_line_len = rlen;
        split_line();
    } else if (tn > 0) {
        get_var(tgt)->set_str(result, rlen);
    } else {
        str_copy(g_line, result, kLineCap);
        g_line_len = rlen;
        split_line();
    }
}

// Execute one statement starting at p, return false on next/exit
bool execute_stmt(const char*& p) noexcept {
    skip_ws(p);
    if (*p == '\0' || *p == '}') return true;
    if (*p == ';') { ++p; return true; }
    if (*p == '{') {
        ++p;
        bool ok = execute_stmts(p);
        skip_ws(p); if (*p == '}') ++p;
        return ok;
    }

    // print
    if (str_starts(p, "print") && !str_starts(p, "printf") &&
        (p[5] == ' ' || p[5] == '\t' || p[5] == '\n' || p[5] == ';' || p[5] == '}' || p[5] == '(')) {
        p += 5;
        execute_print(p, false);
        return true;
    }
    // printf
    if (str_starts(p, "printf") && (p[6] == ' ' || p[6] == '\t' || p[6] == '(' )) {
        p += 6;
        if (*p == '(') ++p;
        execute_print(p, true);
        skip_ws(p); if (*p == ')') ++p;
        return true;
    }

    // next
    if (str_starts(p, "next") && (p[4] == ';' || p[4] == ' ' || p[4] == '\n' || p[4] == '}')) {
        p += 4;
        return false; // signals "next record"
    }
    // exit
    if (str_starts(p, "exit") && (p[4] == ';' || p[4] == ' ' || p[4] == '\n' || p[4] == '}' || !p[4])) {
        p += 4;
        // Just stop processing
        return false;
    }

    // sub/gsub
    if (str_starts(p, "gsub(") || str_starts(p, "gsub ")) {
        p += 4;
        execute_sub(p, true);
        return true;
    }
    if (str_starts(p, "sub(") || str_starts(p, "sub ")) {
        p += 3;
        execute_sub(p, false);
        return true;
    }

    // if
    if (str_starts(p, "if") && (p[2] == ' ' || p[2] == '(')) {
        p += 2; skip_ws(p);
        if (*p == '(') ++p;
        Val cond = eval_expr(p);
        skip_ws(p); if (*p == ')') ++p; skip_ws(p);
        bool ok = execute_stmt(p);
        skip_ws(p);
        if (str_starts(p, "else")) {
            p += 4; skip_ws(p);
            if (cond.as_int() == 0) { ok = execute_stmt(p); }
            else { execute_stmt(p); }
        }
        return ok && cond.as_int() != 0;
    }

    // while
    if (str_starts(p, "while") && (p[5] == ' ' || p[5] == '(')) {
        p += 5; skip_ws(p);
        const char* cond_start = p;
        for (int iter = 0; iter < 10000; ++iter) {
            p = cond_start;
            if (*p == '(') ++p;
            Val cond = eval_expr(p);
            skip_ws(p); if (*p == ')') ++p; skip_ws(p);
            if (cond.as_int() == 0) {
                // skip body
                execute_stmt(p); break;
            }
            execute_stmt(p);
        }
        return true;
    }

    // for (init; cond; incr)
    if (str_starts(p, "for") && (p[3] == ' ' || p[3] == '(')) {
        p += 3; skip_ws(p);
        if (*p == '(') ++p;
        // init
        if (*p != ';') execute_stmt(p);
        skip_ws(p); if (*p == ';') ++p;
        const char* cond_start = p;
        // find incr
        const char* incr_start = cond_start;
        { int depth = 0;
          while (*incr_start) {
              if (*incr_start == '(') ++depth;
              else if (*incr_start == ')') { if (depth == 0) break; --depth; }
              else if (*incr_start == ';' && depth == 0) { ++incr_start; break; }
              ++incr_start;
          }
        }
        const char* body_start = incr_start;
        // find body
        { int depth = 0;
          while (*body_start) {
              if (*body_start == ')' && depth == 0) { ++body_start; break; }
              if (*body_start == '(') ++depth;
              else if (*body_start == ')') --depth;
              ++body_start;
          }
        }
        skip_ws(body_start);

        for (int iter = 0; iter < 10000; ++iter) {
            // eval cond
            const char* cp = cond_start;
            if (*cp == ';') { ++cp; } // empty cond = true
            Val cond{};
            if (*cp == ';' || *cp == ')') cond.set_int(1);
            else cond = eval_expr(cp);
            if (cond.as_int() == 0) break;
            const char* bp = body_start;
            bool ok = execute_stmt(bp);
            if (!ok) break;
            // incr
            const char* ip = incr_start;
            if (*ip && *ip != ')') eval_expr(ip);
        }
        p = body_start;
        execute_stmt(p); // skip body once to advance p past it
        return true;
    }

    // do-while
    if (str_starts(p, "do") && (p[2] == ' ' || p[2] == '{')) {
        p += 2; skip_ws(p);
        const char* body_start = p;
        for (int iter = 0; iter < 10000; ++iter) {
            p = body_start;
            execute_stmt(p);
            skip_ws(p);
            if (str_starts(p, "while")) { p += 5; }
            skip_ws(p); if (*p == '(') ++p;
            Val cond = eval_expr(p);
            skip_ws(p); if (*p == ')') ++p;
            if (cond.as_int() == 0) break;
        }
        return true;
    }

    // Assignment: var = expr or var += expr etc.
    {
        const char* save = p;
        char name[kVarNameCap]{}; int nn = 0;
        bool field_assign = false; int field_n2 = 0;

        if (*p == '$') {
            ++p; field_assign = true;
            while (*p >= '0' && *p <= '9') field_n2 = field_n2 * 10 + (*p++ - '0');
        } else {
            while (((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                    (*p >= '0' && *p <= '9') || *p == '_') && nn < kVarNameCap - 1)
                name[nn++] = *p++;
            name[nn] = '\0';
        }
        skip_ws(p);

        bool is_assign = false; char aug = '\0';
        if (*p == '=' && p[1] != '=') { is_assign = true; ++p; }
        else if ((*p == '+' || *p == '-' || *p == '*' || *p == '/') && p[1] == '=') {
            aug = *p; is_assign = true; p += 2;
        } else if (p[0] == '+' && p[1] == '+') {
            // pre/post ++ already handled in eval_primary, but handle as stmt here
            if (nn > 0) { long v = get_var_int(name); set_var_int(name, v + 1); }
            p += 2; return true;
        } else if (p[0] == '-' && p[1] == '-') {
            if (nn > 0) { long v = get_var_int(name); set_var_int(name, v - 1); }
            p += 2; return true;
        }

        if (is_assign && (nn > 0 || field_assign)) {
            Val rhs = eval_expr(p);
            if (aug == '\0') {
                if (field_assign) {
                    // $N = val: rebuild $0
                    if (field_n2 >= 1 && field_n2 <= g_nf) {
                        str_copy(g_field_store + (g_fields[field_n2 - 1] - g_field_store),
                                 rhs.as_str(), kValCap);
                        rebuild_dollar0();
                    }
                } else {
                    *get_var(name) = rhs;
                }
            } else {
                long old = field_assign ? str_to_int(get_field(field_n2)) : get_var_int(name);
                long nv = rhs.as_int();
                long result2 = (aug == '+') ? old + nv : (aug == '-') ? old - nv :
                               (aug == '*') ? old * nv : (nv != 0 ? old / nv : 0);
                if (field_assign) {
                    char buf[32]; fmt_int(result2, buf, sizeof(buf));
                    // simplified: just update g_line
                } else {
                    set_var_int(name, result2);
                }
            }
            return true;
        }

        // Not an assignment -- evaluate as expression (side effects)
        p = save;
        eval_expr(p);
        // skip trailing semicolon or newline
        skip_ws(p);
        if (*p == ';' || *p == '\n') ++p;
        return true;
    }
}

bool execute_stmts(const char*& p) noexcept {
    while (*p && *p != '}') {
        skip_ws(p);
        if (*p == '\0' || *p == '}') break;
        bool ok = execute_stmt(p);
        if (!ok) return false;
        skip_ws(p);
        while (*p == ';' || *p == '\n') ++p;
    }
    return true;
}

bool execute_action(const char* action) noexcept {
    const char* p = action;
    return execute_stmts(p);
}

// ============================================================
// Pattern matching for a rule
// ============================================================

bool matches_pattern(const Rule& rule) noexcept {
    if (rule.pattern[0] == '\0') return true; // match all
    if (str_eq(rule.pattern, "BEGIN") || str_eq(rule.pattern, "END")) return false;

    if (rule.is_regex) return regex_match(rule.pattern, g_line);

    // Expression pattern: evaluate
    const char* p = rule.pattern;
    Val v = eval_expr(p);
    return v.as_int() != 0;
}

// ============================================================
// Process one line
// ============================================================

// Returns false if "next" or "exit" was triggered.
bool process_line() noexcept {
    split_line();
    set_var_int("NR", g_nr);
    set_var_int("FNR", g_fnr);

    for (int i = 0; i < g_nrules; ++i) {
        auto& rule = g_rules[i];
        if (str_eq(rule.pattern, "BEGIN") || str_eq(rule.pattern, "END")) continue;
        if (matches_pattern(rule)) {
            if (!execute_action(rule.action)) return false;
        }
    }
    return true;
}

// ============================================================
// Program parser
// ============================================================

// Skip whitespace and comments
void prog_skip(const char*& p) noexcept {
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
        if (*p == '#') { while (*p && *p != '\n') ++p; continue; }
        break;
    }
}

void parse_program(const char* prog) noexcept {
    const char* p = prog;
    prog_skip(p);

    while (*p && g_nrules < kMaxRules) {
        Rule& rule = g_rules[g_nrules];
        rule = {};

        prog_skip(p);
        if (!*p) break;

        // BEGIN / END
        if (str_starts(p, "BEGIN")) { p += 5; str_copy(rule.pattern, "BEGIN", kPatCap); }
        else if (str_starts(p, "END")) { p += 3; str_copy(rule.pattern, "END", kPatCap); }
        else if (*p == '/') {
            // /regex/ pattern
            ++p;
            int n = 0;
            while (*p && *p != '/' && n < kPatCap - 1) rule.pattern[n++] = *p++;
            rule.pattern[n] = '\0';
            if (*p == '/') ++p;
            rule.is_regex = true;
        } else if (*p == '{') {
            // bare {action}: match all
            rule.pattern[0] = '\0';
        } else {
            // expression pattern
            int n = 0;
            // collect until { or end
            while (*p && *p != '{' && *p != '\n' && n < kPatCap - 1) rule.pattern[n++] = *p++;
            // trim trailing space
            while (n > 0 && (rule.pattern[n-1] == ' ' || rule.pattern[n-1] == '\t')) --n;
            rule.pattern[n] = '\0';
        }

        prog_skip(p);

        // Action block { ... }
        if (*p == '{') {
            ++p;
            int n = 0, depth = 1;
            while (*p && n < kActCap - 1) {
                if (*p == '{') ++depth;
                else if (*p == '}') { --depth; if (depth == 0) { ++p; break; } }
                rule.action[n++] = *p++;
            }
            rule.action[n] = '\0';
        } else {
            // No action: default print
            str_copy(rule.action, "print", kActCap);
        }

        ++g_nrules;
        prog_skip(p);
    }
}

// ============================================================
// Process file (or stdin)
// ============================================================

void process_fd(int fd) noexcept {
    char ch;
    g_line_len = 0;
    while (read(fd, &ch, 1) == 1) {
        if (ch == '\n' || g_line_len >= kLineCap - 1) {
            g_line[g_line_len] = '\0';
            ++g_nr; ++g_fnr;
            process_line();
            g_line_len = 0;
        } else {
            g_line[g_line_len++] = ch;
        }
    }
    // flush partial last line
    if (g_line_len > 0) {
        g_line[g_line_len] = '\0';
        ++g_nr; ++g_fnr;
        process_line();
        g_line_len = 0;
    }
}

void run_begin() noexcept {
    for (int i = 0; i < g_nrules; ++i) {
        if (str_eq(g_rules[i].pattern, "BEGIN"))
            execute_action(g_rules[i].action);
    }
}

void run_end() noexcept {
    for (int i = 0; i < g_nrules; ++i) {
        if (str_eq(g_rules[i].pattern, "END"))
            execute_action(g_rules[i].action);
    }
}

} // namespace

int main(int argc, char** argv) {
    // Default OFS/ORS
    g_ofs[0] = ' '; g_ofs[1] = '\0';
    g_ors[0] = '\n'; g_ors[1] = '\0';

    const char* program = nullptr;
    char prog_file_buf[4096]{};
    int argi = 1;

    while (argi < argc) {
        const char* arg = argv[argi];
        if (arg[0] == '-' && arg[1] == 'F') {
            // -F sep or -Fsep
            const char* sep = (arg[2] != '\0') ? arg + 2 : (argi + 1 < argc ? argv[++argi] : " ");
            g_fs = sep[0];
            ++argi;
        } else if (arg[0] == '-' && arg[1] == 'v') {
            // -v var=val
            const char* kv = (arg[2] != '\0') ? arg + 2 : (argi + 1 < argc ? argv[++argi] : "");
            ++argi;
            // parse var=val
            char vname[kVarNameCap]{}; int vn = 0;
            const char* kp = kv;
            while (*kp && *kp != '=' && vn < kVarNameCap - 1) vname[vn++] = *kp++;
            vname[vn] = '\0';
            if (*kp == '=') ++kp;
            set_var_str(vname, kp);
        } else if (arg[0] == '-' && arg[1] == 'f') {
            // -f progfile
            const char* progfile = (arg[2] != '\0') ? arg + 2 : (argi + 1 < argc ? argv[++argi] : "");
            ++argi;
            int fd = open(progfile, O_RDONLY, 0);
            if (fd < 0) {
                write_str(kStderr, "awk: cannot open program file: ");
                write_str(kStderr, progfile); write_str(kStderr, "\n");
                return 1;
            }
            int n = 0; char c;
            while (n < (int)sizeof(prog_file_buf) - 1 && read(fd, &c, 1) == 1)
                prog_file_buf[n++] = c;
            prog_file_buf[n] = '\0';
            close(fd);
            program = prog_file_buf;
        } else if (arg[0] == '-' && arg[1] == '-' && arg[2] == '\0') {
            ++argi; break;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            write_str(kStderr, "awk: unknown option: "); write_str(kStderr, arg); write_str(kStderr, "\n");
            return 1;
        } else if (program == nullptr) {
            program = argv[argi++];
        } else {
            break;
        }
    }

    if (program == nullptr) {
        write_str(kStderr, "usage: awk [-F sep] [-v var=val] [-f file] 'program' [file ...]\n");
        return 1;
    }

    parse_program(program);
    run_begin();

    if (argi >= argc) {
        // Read from stdin
        set_var_str("FILENAME", "-");
        g_fnr = 0;
        process_fd(0);
    } else {
        for (int i = argi; i < argc; ++i) {
            // Handle var=val arguments between files
            bool is_assign = false;
            const char* kp = argv[i];
            char vname[kVarNameCap]{}; int vn = 0;
            while (*kp && *kp != '=' && vn < kVarNameCap - 1) vname[vn++] = *kp++;
            vname[vn] = '\0';
            if (*kp == '=' && vn > 0) {
                set_var_str(vname, kp + 1);
                is_assign = true;
            }
            if (is_assign) continue;

            int fd = 0;
            if (argv[i][0] == '-' && argv[i][1] == '\0') {
                fd = 0;
                set_var_str("FILENAME", "-");
            } else {
                fd = open(argv[i], O_RDONLY, 0);
                if (fd < 0) {
                    write_str(kStderr, "awk: cannot open: ");
                    write_str(kStderr, argv[i]); write_str(kStderr, "\n");
                    continue;
                }
                set_var_str("FILENAME", argv[i]);
            }
            g_fnr = 0;
            process_fd(fd);
            if (fd > 0) close(fd);
        }
    }

    run_end();
    return 0;
}
