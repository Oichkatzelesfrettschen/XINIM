#include <unistd.h>

int main(void) {
    if (isatty(0)) {
        write(1, "/dev/tty\n", 9);
        return 0;
    }
    write(1, "not a tty\n", 10);
    return 1;
}
