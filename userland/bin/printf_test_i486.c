#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_pass = 0, g_fail = 0;

static void check(const char *name, const char *got, const char *expected) {
    if (strcmp(got, expected) == 0) {
        printf("  PASS: %s\n", name);
        ++g_pass;
    } else {
        printf("  FAIL: %s (got '%s', expected '%s')\n", name, got, expected);
        ++g_fail;
    }
}

int main(void) {
    printf("=== printf Verification Test ===\n");
    char buf[128];

    snprintf(buf, sizeof(buf), "%d", 42);
    check("%d positive", buf, "42");

    snprintf(buf, sizeof(buf), "%d", -7);
    check("%d negative", buf, "-7");

    snprintf(buf, sizeof(buf), "%d", 0);
    check("%d zero", buf, "0");

    snprintf(buf, sizeof(buf), "%u", 4294967295U);
    check("%u max", buf, "4294967295");

    snprintf(buf, sizeof(buf), "%x", 255);
    check("%x", buf, "ff");

    snprintf(buf, sizeof(buf), "%X", 255);
    check("%X", buf, "FF");

    snprintf(buf, sizeof(buf), "%o", 8);
    check("%o", buf, "10");

    snprintf(buf, sizeof(buf), "%s", "hello");
    check("%s", buf, "hello");

    snprintf(buf, sizeof(buf), "%c", 'A');
    check("%c", buf, "A");

    snprintf(buf, sizeof(buf), "%%");
    check("%%", buf, "%");

    snprintf(buf, sizeof(buf), "%5d", 42);
    check("%5d padded", buf, "   42");

    snprintf(buf, sizeof(buf), "%-5d|", 42);
    check("%-5d left", buf, "42   |");

    snprintf(buf, sizeof(buf), "%05d", 42);
    check("%05d zero-padded", buf, "00042");

    printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
