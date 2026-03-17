#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

/*
 * POSIX compliance test runner.
 * Runs each test program in sequence and reports pass/fail.
 */

static void write_str(const char *s) { write(1, s, strlen(s)); }
static void write_num(int n) {
    char buf[12]; int pos = 0;
    if (n == 0) buf[pos++] = '0';
    else { while (n > 0) { buf[pos++] = (char)('0' + n % 10); n /= 10; } }
    for (int i = 0; i < pos/2; ++i) { char t=buf[i]; buf[i]=buf[pos-1-i]; buf[pos-1-i]=t; }
    write(1, buf, pos);
}

static int run_test(const char *name, const char *path) {
    write_str("  Running ");
    write_str(name);
    write_str("... ");

    int pid = fork();
    if (pid == 0) {
        char *argv[] = {(char*)path, 0};
        execve(path, argv, 0);
        _exit(127);
    }
    if (pid < 0) {
        write_str("SKIP (fork failed)\n");
        return -1;
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        write_str("PASS\n");
        return 0;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        write_str("SKIP (not found)\n");
        return -1;
    }
    write_str("FAIL (exit=");
    write_num(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    write_str(")\n");
    return 1;
}

int main(void) {
    write_str("========================================\n");
    write_str("  XINIM i486 POSIX Compliance Tests\n");
    write_str("========================================\n\n");

    int pass = 0, fail = 0, skip = 0;

    struct { const char *name; const char *path; } tests[] = {
        {"signal delivery",   "/bin/signal_test"},
        {"printf formatting", "/bin/printf_test"},
        {"fork/exec/wait",    "/bin/forkexec_test"},
        {"pipe and redirect", "/bin/pipe_test"},
        {"ext2 mutation",     "/bin/ext2_stress"},
        {"preemption",        "/bin/preempt_test"},
        {"Ctrl+C signal",     "/bin/ctrlc_test"},
        {0, 0}
    };

    for (int i = 0; tests[i].name != 0; ++i) {
        int r = run_test(tests[i].name, tests[i].path);
        if (r == 0) ++pass;
        else if (r > 0) ++fail;
        else ++skip;
    }

    write_str("\n========================================\n");
    write_str("  Results: ");
    write_num(pass); write_str(" pass, ");
    write_num(fail); write_str(" fail, ");
    write_num(skip); write_str(" skip\n");
    write_str("========================================\n");

    return fail > 0 ? 1 : 0;
}
