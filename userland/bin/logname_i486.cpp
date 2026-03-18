// logname -- print login name (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// XINIM single-user system: always "root".

#include <unistd.h>

int main() {
    write(1, "root\n", 5);
    return 0;
}
