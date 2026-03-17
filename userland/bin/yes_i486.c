#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *msg = (argc > 1) ? argv[1] : "y";
    size_t len = strlen(msg);
    for (;;) {
        if (write(1, msg, len) <= 0) break;
        if (write(1, "\n", 1) <= 0) break;
    }
    return 0;
}
