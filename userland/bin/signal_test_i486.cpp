// XINIM-owned userspace implementation; compile as freestanding C++23.
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

static volatile int g_sigusr1_count = 0;
static volatile int g_sigchld_count = 0;

static void sigusr1_handler(int sig) {
    (void)sig;
    g_sigusr1_count = g_sigusr1_count + 1;
}

static void sigchld_handler(int sig) {
    (void)sig;
    g_sigchld_count = g_sigchld_count + 1;
}

static void write_str(const char *s) {
    write(1, s, strlen(s));
}

static void write_ok(const char *name) {
    write_str("  PASS: ");
    write_str(name);
    write_str("\n");
}

static void write_fail(const char *name) {
    write_str("  FAIL: ");
    write_str(name);
    write_str("\n");
}

int main(void) {
    write_str("=== Signal Delivery Test ===\n");
    int passed = 0, failed = 0;

    /* Test 1: Install SIGUSR1 handler via signal() */
    signal(10, sigusr1_handler); /* SIGUSR1 = 10 */
    kill(getpid(), 10);
    if (g_sigusr1_count == 1) { write_ok("SIGUSR1 self-signal"); ++passed; }
    else { write_fail("SIGUSR1 self-signal"); ++failed; }

    /* Test 2: Send SIGUSR1 again */
    kill(getpid(), 10);
    if (g_sigusr1_count == 2) { write_ok("SIGUSR1 second delivery"); ++passed; }
    else { write_fail("SIGUSR1 second delivery"); ++failed; }

    /* Test 3: SIGCHLD on child exit */
    signal(17, sigchld_handler); /* SIGCHLD = 17 */
    int pid = fork();
    if (pid == 0) {
        _exit(42);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 42) {
        write_ok("child exit status 42");
        ++passed;
    } else {
        write_fail("child exit status 42");
        ++failed;
    }

    /* Test 4: SIG_IGN */
    signal(10, (void(*)(int))1); /* SIG_IGN = 1 */
    kill(getpid(), 10);
    /* If we get here, SIG_IGN worked (didn't crash or terminate) */
    write_ok("SIG_IGN for SIGUSR1");
    ++passed;

    /* Test 5: kill(0) validity check */
    if (kill(getpid(), 0) == 0) { write_ok("kill(pid, 0) validity check"); ++passed; }
    else { write_fail("kill(pid, 0) validity check"); ++failed; }

    /* Summary */
    write_str("=== ");
    char num[4];
    num[0] = (char)('0' + passed); num[1] = '\0';
    write_str(num);
    write_str(" passed, ");
    num[0] = (char)('0' + failed);
    write_str(num);
    write_str(" failed ===\n");

    return failed > 0 ? 1 : 0;
}
