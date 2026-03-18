// clear -- clear the terminal screen
// Cleanroom C++23 implementation.
// Outputs ANSI escape sequences to clear screen and home cursor.

#include <unistd.h>

int main() {
    const char seq[] = "\033[2J\033[H";
    write(1, seq, sizeof(seq) - 1);
    return 0;
}
