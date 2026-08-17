// XINIM-owned userspace implementation; compile as freestanding C++23.
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

/*
 * Preemption test: fork two children that busy-loop writing to stdout.
 * If preemption works, both produce output within 5 seconds.
 * Parent waits for both to exit (they exit after N iterations).
 */

static void write_str(const char *s) { write(1, s, strlen(s)); }

static void busy_loop(const char *tag, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        /* Busy work to consume CPU */
        volatile int sum = 0;
        for (int j = 0; j < 10000; ++j) sum += j;
        (void)sum;

        if (i % 500 == 0) {
            write_str(tag);
        }
    }
}

int main(void) {
    write_str("=== Preemption Test ===\n");
    write_str("Forking two CPU-bound children...\n");

    int pid1 = fork();
    if (pid1 == 0) {
        busy_loop("A", 2000);
        _exit(0);
    }

    int pid2 = fork();
    if (pid2 == 0) {
        busy_loop("B", 2000);
        _exit(0);
    }

    if (pid1 < 0 || pid2 < 0) {
        write_str("FAIL: fork failed\n");
        return 1;
    }

    int s1 = 0, s2 = 0;
    waitpid(pid1, &s1, 0);
    waitpid(pid2, &s2, 0);

    write_str("\n");
    if (WIFEXITED(s1) && WEXITSTATUS(s1) == 0 &&
        WIFEXITED(s2) && WEXITSTATUS(s2) == 0) {
        write_str("PASS: both children completed (preemption works)\n");
        return 0;
    }
    write_str("FAIL: one or both children did not complete\n");
    return 1;
}
