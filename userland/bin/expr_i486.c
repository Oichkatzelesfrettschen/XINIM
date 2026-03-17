#include <unistd.h>
#include <string.h>

static void write_long(int fd, long val) {
    char buf[24];
    int pos = 0;
    int neg = 0;
    unsigned long uval;
    if (val < 0) { neg = 1; uval = (unsigned long)(-val); }
    else uval = (unsigned long)val;
    if (uval == 0) buf[pos++] = '0';
    else while (uval > 0) { buf[pos++] = (char)('0' + uval % 10); uval /= 10; }
    if (neg) buf[pos++] = '-';
    for (int i = 0; i < pos / 2; ++i) {
        char t = buf[i]; buf[i] = buf[pos-1-i]; buf[pos-1-i] = t;
    }
    write(fd, buf, pos);
}

static long parse_long(const char *s) {
    long v = 0;
    int neg = 0;
    if (*s == '-') { neg = 1; ++s; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); ++s; }
    return neg ? -v : v;
}

int main(int argc, char **argv) {
    if (argc == 2) {
        /* Single argument: print it */
        write(1, argv[1], strlen(argv[1]));
        write(1, "\n", 1);
        return (argv[1][0] == '0' && argv[1][1] == '\0') ? 1 : 0;
    }
    if (argc == 4) {
        /* expr A OP B */
        long a = parse_long(argv[1]);
        long b = parse_long(argv[3]);
        const char *op = argv[2];
        long result = 0;
        if (strcmp(op, "+") == 0) result = a + b;
        else if (strcmp(op, "-") == 0) result = a - b;
        else if (strcmp(op, "*") == 0) result = a * b;
        else if (strcmp(op, "/") == 0) { if (b == 0) { write(2, "expr: division by zero\n", 23); return 2; } result = a / b; }
        else if (strcmp(op, "%") == 0) { if (b == 0) { write(2, "expr: division by zero\n", 23); return 2; } result = a % b; }
        else if (strcmp(op, "=") == 0) { result = (a == b) ? 1 : 0; }
        else if (strcmp(op, "!=") == 0) { result = (a != b) ? 1 : 0; }
        else if (strcmp(op, "<") == 0) { result = (a < b) ? 1 : 0; }
        else if (strcmp(op, "<=") == 0) { result = (a <= b) ? 1 : 0; }
        else if (strcmp(op, ">") == 0) { result = (a > b) ? 1 : 0; }
        else if (strcmp(op, ">=") == 0) { result = (a >= b) ? 1 : 0; }
        else { write(2, "expr: unknown operator\n", 23); return 2; }
        write_long(1, result);
        write(1, "\n", 1);
        return (result == 0) ? 1 : 0;
    }
    write(2, "usage: expr ARG | expr ARG OP ARG\n", 34);
    return 2;
}
