// test -- evaluate conditional expression (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Also known as [. Supports: -f, -d, -e, -r, -w, -x, -z, -n,
// string =, !=, integer -eq/-ne/-lt/-gt/-le/-ge, -a, -o, !

#include <sys/stat.h>
#include <unistd.h>

namespace {

bool streq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

int slen(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

long parse_long(const char* s) {
    long v = 0;
    bool neg = false;
    if (*s == '-') { neg = true; ++s; }
    else if (*s == '+') { ++s; }
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        ++s;
    }
    return neg ? -v : v;
}

// Recursive descent parser for test expressions.
// Grammar:
//   expr    = or_expr
//   or_expr = and_expr ( -o and_expr )*
//   and_expr = not_expr ( -a not_expr )*
//   not_expr = ! not_expr | primary
//   primary  = unary_op OPERAND
//            | OPERAND binary_op OPERAND
//            | OPERAND
//            | ( expr )

int g_argc;
char** g_argv;
int g_pos;

const char* peek() {
    if (g_pos >= g_argc) return "";
    return g_argv[g_pos];
}

const char* advance() {
    if (g_pos >= g_argc) return "";
    return g_argv[g_pos++];
}

bool eval_expr();

bool eval_primary() {
    const char* tok = peek();

    // Parenthesized expression
    if (streq(tok, "(")) {
        advance(); // consume (
        bool result = eval_expr();
        if (streq(peek(), ")")) advance(); // consume )
        return result;
    }

    // Unary file tests
    if (tok[0] == '-' && tok[2] == '\0' && g_pos + 1 < g_argc) {
        char op = tok[1];
        if (op == 'f' || op == 'd' || op == 'e' ||
            op == 'r' || op == 'w' || op == 'x') {
            advance(); // consume operator
            const char* path = advance();
            struct stat st{};
            if (stat(path, &st) != 0) return false;
            switch (op) {
            case 'e': return true;
            case 'f': return S_ISREG(st.st_mode);
            case 'd': return S_ISDIR(st.st_mode);
            case 'r': return (access(path, R_OK) == 0);
            case 'w': return (access(path, W_OK) == 0);
            case 'x': return (access(path, X_OK) == 0);
            }
            return false;
        }

        // Unary string tests
        if (op == 'z' || op == 'n') {
            advance(); // consume operator
            const char* operand = advance();
            int len = slen(operand);
            return (op == 'z') ? (len == 0) : (len > 0);
        }
    }

    // We have an operand. Look ahead for binary operator.
    advance(); // consume first operand

    const char* next = peek();

    // String comparison
    if (streq(next, "=")) {
        advance(); // consume =
        const char* rhs = advance();
        return streq(tok, rhs);
    }
    if (streq(next, "!=")) {
        advance(); // consume !=
        const char* rhs = advance();
        return !streq(tok, rhs);
    }

    // Integer comparison
    if (streq(next, "-eq") || streq(next, "-ne") ||
        streq(next, "-lt") || streq(next, "-gt") ||
        streq(next, "-le") || streq(next, "-ge")) {
        advance(); // consume operator
        const char* rhs_str = advance();
        long lhs = parse_long(tok);
        long rhs = parse_long(rhs_str);
        if (streq(next, "-eq")) return lhs == rhs;
        if (streq(next, "-ne")) return lhs != rhs;
        if (streq(next, "-lt")) return lhs < rhs;
        if (streq(next, "-gt")) return lhs > rhs;
        if (streq(next, "-le")) return lhs <= rhs;
        if (streq(next, "-ge")) return lhs >= rhs;
    }

    // Bare string: true if non-empty
    return slen(tok) > 0;
}

bool eval_not() {
    if (streq(peek(), "!")) {
        advance();
        return !eval_not();
    }
    return eval_primary();
}

bool eval_and() {
    bool result = eval_not();
    while (streq(peek(), "-a")) {
        advance();
        bool rhs = eval_not();
        result = result && rhs;
    }
    return result;
}

bool eval_expr() {
    bool result = eval_and();
    while (streq(peek(), "-o")) {
        advance();
        bool rhs = eval_and();
        result = result || rhs;
    }
    return result;
}

} // namespace

int main(int argc, char** argv) {
    // If invoked as "[", strip trailing "]"
    if (argc > 0) {
        // Check if basename is "["
        const char* base = argv[0];
        for (const char* p = argv[0]; *p; ++p)
            if (*p == '/' && *(p + 1) != '\0') base = p + 1;
        if (base[0] == '[' && base[1] == '\0') {
            if (argc > 1 && streq(argv[argc - 1], "]")) {
                --argc; // remove trailing ]
            }
        }
    }

    if (argc <= 1) return 1; // no expression = false

    g_argc = argc;
    g_argv = argv;
    g_pos = 1; // skip argv[0]

    return eval_expr() ? 0 : 1;
}
