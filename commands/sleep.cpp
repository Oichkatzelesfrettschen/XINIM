/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* sleep - suspend a process for x sec		Author: Andy Tanenbaum */

// Entry point for the sleep command
int main(int argc, char *argv[]) {
    int seconds = 0; // Number of seconds to sleep

    if (argc != 2) {
        std_err("Usage: sleep time\n");
        return 1;
    }

    // Convert numeric argument from string to integer
    for (const char *p = argv[1]; *p != '\0'; ++p) {
        const char c = *p;
        if (c < '0' || c > '9') {
            std_err("sleep: bad arg\n");
            return 1;
        }
        seconds = 10 * seconds + (c - '0');
    }

    // Sleep for the requested duration
    sleep(seconds);
    return 0;
}
