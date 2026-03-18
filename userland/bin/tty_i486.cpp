// tty -- print terminal name (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// If stdin is a tty, print "/dev/tty"; otherwise "not a tty".

#include <unistd.h>

int main() {
    if (isatty(0)) {
        write(1, "/dev/tty\n", 9);
        return 0;
    }
    write(1, "not a tty\n", 10);
    return 1;
}
