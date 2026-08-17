// XINIM-owned userspace implementation; compile as freestanding C++23.
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

static void write_str(const char *s) { write(1, s, strlen(s)); }
static int g_pass = 0, g_fail = 0;

static void check(const char *name, int cond) {
    if (cond) { write_str("  PASS: "); ++g_pass; }
    else { write_str("  FAIL: "); ++g_fail; }
    write_str(name);
    write_str("\n");
}

int main(void) {
    write_str("=== Pipe and Redirect Test ===\n");

    /* Test 1: Basic pipe */
    {
        int fds[2];
        int r = pipe(fds);
        check("pipe() returns 0", r == 0);

        const char *msg = "hello pipe";
        write(fds[1], msg, strlen(msg));
        close(fds[1]);

        char buf[32] = {0};
        ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
        close(fds[0]);
        check("pipe read returns correct data", n == 10 && strcmp(buf, "hello pipe") == 0);
    }

    /* Test 2: Pipe between parent and child */
    {
        int fds[2];
        pipe(fds);
        int pid = fork();
        if (pid == 0) {
            close(fds[0]);
            const char *msg = "from child";
            write(fds[1], msg, strlen(msg));
            close(fds[1]);
            _exit(0);
        }
        close(fds[1]);
        char buf[32] = {0};
        ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
        close(fds[0]);
        int status;
        waitpid(pid, &status, 0);
        check("pipe child->parent transfer", n == 10 && strcmp(buf, "from child") == 0);
    }

    /* Test 3: File redirect (write to file, read back) */
    {
        int fd = open("/tmp/pipe_test_out", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        check("open for write", fd >= 0);
        if (fd >= 0) {
            write(fd, "redirect test\n", 14);
            close(fd);

            fd = open("/tmp/pipe_test_out", O_RDONLY, 0);
            check("open for read", fd >= 0);
            if (fd >= 0) {
                char buf[32] = {0};
                ssize_t n = read(fd, buf, sizeof(buf) - 1);
                close(fd);
                check("file content matches", n == 14 && strncmp(buf, "redirect test", 13) == 0);
            }
            unlink("/tmp/pipe_test_out");
        }
    }

    /* Test 4: dup2 redirect */
    {
        int fds[2];
        pipe(fds);
        int pid = fork();
        if (pid == 0) {
            close(fds[0]);
            dup2(fds[1], 1); /* stdout -> pipe write end */
            close(fds[1]);
            const char *msg = "dup2 works";
            write(1, msg, strlen(msg)); /* writes to pipe via dup2'd stdout */
            _exit(0);
        }
        close(fds[1]);
        char buf[32] = {0};
        ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
        close(fds[0]);
        int status;
        waitpid(pid, &status, 0);
        check("dup2 stdout redirect", n == 10 && strcmp(buf, "dup2 works") == 0);
    }

    write_str("=== ");
    char num[4]; num[0] = (char)('0' + g_pass); num[1] = '\0';
    write_str(num);
    write_str(" passed, ");
    num[0] = (char)('0' + g_fail);
    write_str(num);
    write_str(" failed ===\n");
    return g_fail > 0 ? 1 : 0;
}
