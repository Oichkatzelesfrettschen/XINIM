// XINIM-owned userspace implementation; compile as freestanding C++23.
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>

/*
 * Ctrl+C test: install SIGINT handler, then sleep.
 * If Ctrl+C is sent (or kill -2), handler fires and we report success.
 * Can be tested manually: run this, press Ctrl+C within 5 seconds.
 * Or automated: fork, child sleeps, parent sends SIGINT after 1 second.
 */

static volatile int g_got_sigint = 0;

static void sigint_handler(int sig) {
    (void)sig;
    g_got_sigint = 1;
}

static void write_str(const char *s) { write(1, s, strlen(s)); }

int main(void) {
    write_str("=== Ctrl+C Signal Test ===\n");

    signal(2, sigint_handler); /* SIGINT = 2 */

    int pid = fork();
    if (pid == 0) {
        /* Child: wait a moment then send SIGINT to parent */
        struct timespec ts = {0, 500000000}; /* 0.5s */
        nanosleep(&ts, 0);
        kill(getppid(), 2); /* SIGINT */
        _exit(0);
    }

    /* Parent: sleep up to 3 seconds, expecting SIGINT to interrupt */
    struct timespec ts2 = {3, 0};
    nanosleep(&ts2, 0);

    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }

    if (g_got_sigint) {
        write_str("PASS: SIGINT handler was invoked\n");
        return 0;
    }
    write_str("FAIL: SIGINT handler was NOT invoked\n");
    return 1;
}
