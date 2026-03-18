// expr -- evaluate expressions (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Supports: integer arithmetic (+, -, *, /, %), comparison (=, !=, <, >, <=, >=),
// logical (|, &), string matching (:), parentheses.
// Recursive descent parser with proper operator precedence.
// Exit 0 if result is non-null and non-zero, 1 if null/zero, 2 on error.

#include <string.h>
#include <unistd.h>

namespace {

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

void write_long(int fd, long val) {
    char buf[24];
    int pos = 0;
    bool neg = false;
    unsigned long uval;
    if (val < 0) { neg = true; uval = static_cast<unsigned long>(-val); }
    else uval = static_cast<unsigned long>(val);
    if (uval == 0) { buf[pos++] = '0'; }
    else { while (uval > 0) { buf[pos++] = static_cast<char>('0' + uval % 10); uval /= 10; } }
    if (neg) buf[pos++] = '-';
    for (int i = pos - 1; i >= 0; --i)
        write_all(fd, &buf[i], 1);
}

int str_len(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

int str_cmp(const char* a, const char* b) {
    while (*a != '\0' && *a == *b) { ++a; ++b; }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

bool is_integer(const char* s) {
    if (*s == '-' || *s == '+') ++s;
    if (*s == '\0') return false;
    while (*s != '\0') {
        if (*s < '0' || *s > '9') return false;
        ++s;
    }
    return true;
}

long to_long(const char* s) {
    long val = 0;
    bool neg = false;
    if (*s == '-') { neg = true; ++s; }
    else if (*s == '+') ++s;
    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); ++s; }
    return neg ? -val : val;
}

// Value type: either an integer or a string
struct Val {
    long ival;
    char sval[256];
    bool is_num;
};

Val make_num(long v) {
    Val r;
    r.ival = v;
    r.sval[0] = '\0';
    r.is_num = true;
    return r;
}

Val make_str(const char* s) {
    Val r;
    r.is_num = false;
    r.ival = 0;
    int i = 0;
    while (s[i] != '\0' && i < 255) { r.sval[i] = s[i]; ++i; }
    r.sval[i] = '\0';
    if (is_integer(s)) { r.ival = to_long(s); r.is_num = true; }
    return r;
}

bool val_is_null(const Val& v) {
    if (v.is_num) return v.ival == 0;
    return v.sval[0] == '\0';
}

// Parser state
int g_argc;
char** g_argv;
int g_pos;
bool g_error;

const char* cur_tok() {
    if (g_pos >= g_argc) return nullptr;
    return g_argv[g_pos];
}

const char* next_tok() {
    if (g_pos >= g_argc) return nullptr;
    return g_argv[g_pos++];
}

bool tok_is(const char* s) {
    const char* t = cur_tok();
    if (!t) return false;
    return str_cmp(t, s) == 0;
}

// Forward declarations for recursive descent
Val parse_or();
Val parse_and();
Val parse_compare();
Val parse_add();
Val parse_mul();
Val parse_match();
Val parse_primary();

// expr : or_expr
// or_expr : and_expr ( '|' and_expr )*
Val parse_or() {
    Val left = parse_and();
    while (tok_is("|")) {
        next_tok();
        Val right = parse_and();
        if (!val_is_null(left)) { /* keep left */ }
        else left = right;
    }
    return left;
}

// and_expr : compare_expr ( '&' compare_expr )*
Val parse_and() {
    Val left = parse_compare();
    while (tok_is("&")) {
        next_tok();
        Val right = parse_compare();
        if (!val_is_null(left) && !val_is_null(right)) { /* keep left */ }
        else left = make_num(0);
    }
    return left;
}

// compare_expr : add_expr ( ('=' | '!=' | '<' | '<=' | '>' | '>=') add_expr )?
Val parse_compare() {
    Val left = parse_add();
    const char* t = cur_tok();
    if (!t) return left;

    bool is_cmp = (str_cmp(t, "=") == 0 || str_cmp(t, "!=") == 0 ||
                   str_cmp(t, "<") == 0 || str_cmp(t, "<=") == 0 ||
                   str_cmp(t, ">") == 0 || str_cmp(t, ">=") == 0);
    if (!is_cmp) return left;

    const char* op = next_tok();
    Val right = parse_add();

    // Compare as integers if both are numeric, otherwise as strings
    int cmp;
    if (left.is_num && right.is_num) {
        cmp = (left.ival > right.ival) - (left.ival < right.ival);
    } else {
        // Convert to string for comparison
        char ls[256], rs[256];
        if (left.is_num) {
            // Format number to string
            int p = 0;
            long v = left.ival;
            bool neg = v < 0;
            unsigned long uv = neg ? static_cast<unsigned long>(-v) : static_cast<unsigned long>(v);
            if (uv == 0) ls[p++] = '0';
            else { char tmp[20]; int tn = 0; while (uv > 0) { tmp[tn++] = static_cast<char>('0' + uv % 10); uv /= 10; } if (neg) ls[p++] = '-'; for (int i = tn - 1; i >= 0; --i) ls[p++] = tmp[i]; }
            ls[p] = '\0';
        } else {
            int i = 0; while (left.sval[i] && i < 255) { ls[i] = left.sval[i]; ++i; } ls[i] = '\0';
        }
        if (right.is_num) {
            int p = 0;
            long v = right.ival;
            bool neg = v < 0;
            unsigned long uv = neg ? static_cast<unsigned long>(-v) : static_cast<unsigned long>(v);
            if (uv == 0) rs[p++] = '0';
            else { char tmp[20]; int tn = 0; while (uv > 0) { tmp[tn++] = static_cast<char>('0' + uv % 10); uv /= 10; } if (neg) rs[p++] = '-'; for (int i = tn - 1; i >= 0; --i) rs[p++] = tmp[i]; }
            rs[p] = '\0';
        } else {
            int i = 0; while (right.sval[i] && i < 255) { rs[i] = right.sval[i]; ++i; } rs[i] = '\0';
        }
        cmp = str_cmp(ls, rs);
    }

    bool result = false;
    if (str_cmp(op, "=") == 0)       result = (cmp == 0);
    else if (str_cmp(op, "!=") == 0) result = (cmp != 0);
    else if (str_cmp(op, "<") == 0)  result = (cmp < 0);
    else if (str_cmp(op, "<=") == 0) result = (cmp <= 0);
    else if (str_cmp(op, ">") == 0)  result = (cmp > 0);
    else if (str_cmp(op, ">=") == 0) result = (cmp >= 0);

    return make_num(result ? 1 : 0);
}

// add_expr : mul_expr ( ('+' | '-') mul_expr )*
Val parse_add() {
    Val left = parse_mul();
    while (tok_is("+") || tok_is("-")) {
        const char* op = next_tok();
        Val right = parse_mul();
        if (!left.is_num || !right.is_num) {
            write_str(2, "expr: non-numeric argument\n");
            g_error = true;
            return make_num(0);
        }
        if (op[0] == '+') left = make_num(left.ival + right.ival);
        else               left = make_num(left.ival - right.ival);
    }
    return left;
}

// mul_expr : match_expr ( ('*' | '/' | '%') match_expr )*
Val parse_mul() {
    Val left = parse_match();
    while (tok_is("*") || tok_is("/") || tok_is("%")) {
        const char* op = next_tok();
        Val right = parse_match();
        if (!left.is_num || !right.is_num) {
            write_str(2, "expr: non-numeric argument\n");
            g_error = true;
            return make_num(0);
        }
        if (op[0] == '*') {
            left = make_num(left.ival * right.ival);
        } else {
            if (right.ival == 0) {
                write_str(2, "expr: division by zero\n");
                g_error = true;
                return make_num(0);
            }
            if (op[0] == '/') left = make_num(left.ival / right.ival);
            else               left = make_num(left.ival % right.ival);
        }
    }
    return left;
}

// match_expr : primary ( ':' primary )?
// String matching: returns length of matching prefix
Val parse_match() {
    Val left = parse_primary();
    if (tok_is(":")) {
        next_tok();
        Val right = parse_primary();
        // Simple prefix match: count how many chars of right.sval match left.sval prefix
        const char* s = left.is_num ? "" : left.sval;
        const char* p = right.is_num ? "" : right.sval;
        // If left is numeric, format it
        char sbuf[256];
        if (left.is_num) {
            int pos = 0;
            long v = left.ival;
            bool neg = v < 0;
            unsigned long uv = neg ? static_cast<unsigned long>(-v) : static_cast<unsigned long>(v);
            if (uv == 0) sbuf[pos++] = '0';
            else { char tmp[20]; int tn = 0; while (uv > 0) { tmp[tn++] = static_cast<char>('0' + uv % 10); uv /= 10; } if (neg) sbuf[pos++] = '-'; for (int i = tn - 1; i >= 0; --i) sbuf[pos++] = tmp[i]; }
            sbuf[pos] = '\0';
            s = sbuf;
        }
        // Match pattern anchored at start of string
        // Simple fixed-string prefix match (not full regex)
        int slen = str_len(s);
        int plen = str_len(p);
        int mlen = slen < plen ? slen : plen;
        int matched = 0;
        for (int i = 0; i < mlen; ++i) {
            if (s[i] == p[i] || p[i] == '.') ++matched;
            else break;
        }
        return make_num(matched);
    }
    return left;
}

// primary : '(' expr ')' | TOKEN
Val parse_primary() {
    if (tok_is("(")) {
        next_tok(); // consume '('
        Val v = parse_or();
        if (tok_is(")")) next_tok(); // consume ')'
        else { write_str(2, "expr: missing ')'\n"); g_error = true; }
        return v;
    }

    const char* t = next_tok();
    if (!t) {
        write_str(2, "expr: missing operand\n");
        g_error = true;
        return make_num(0);
    }
    return make_str(t);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: expr expression\n");
        return 2;
    }

    g_argc = argc;
    g_argv = argv;
    g_pos = 1;
    g_error = false;

    Val result = parse_or();

    if (g_error) return 2;
    if (g_pos < argc) {
        write_str(2, "expr: syntax error\n");
        return 2;
    }

    // Output result
    if (result.is_num) {
        write_long(1, result.ival);
    } else {
        write_str(1, result.sval);
    }
    write_all(1, "\n", 1);

    // Exit code: 0 if non-null/non-zero, 1 if null/zero
    return val_is_null(result) ? 1 : 0;
}
