// XINIM-owned userspace implementation; compile as freestanding C++23.
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

static void write_str(const char *s) { write(1, s, strlen(s)); }
static void write_num(int n) {
    char buf[12]; size_t pos = 0;
    if (n == 0) buf[pos++] = '0';
    else { while (n > 0) { buf[pos++] = (char)('0' + n % 10); n /= 10; } }
    for (size_t i = 0; i < pos / 2; ++i) { char t=buf[i]; buf[i]=buf[pos-1-i]; buf[pos-1-i]=t; }
    write(1, buf, pos);
}

int main(void) {
    write_str("=== Fork/Exec Stress Test ===\n");
    int success = 0, fail = 0;
    const int N = 8; /* Number of children to fork */

    for (int i = 0; i < N; ++i) {
        int pid = fork();
        if (pid < 0) {
            write_str("  fork failed at iteration ");
            write_num(i);
            write_str("\n");
            ++fail;
            continue;
        }
        if (pid == 0) {
            /* Child: exec /bin/true */
            char true_path[] = "/bin/true";
            char *argv[] = {true_path, nullptr};
            execve("/bin/true", argv, 0);
            _exit(127); /* exec failed */
        }
        /* Parent: wait */
        int status = 0;
        int wpid = waitpid(pid, &status, 0);
        if (wpid == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            ++success;
        } else {
            write_str("  child ");
            write_num(i);
            write_str(" failed (status=");
            write_num(status);
            write_str(")\n");
            ++fail;
        }
    }

    write_str("=== ");
    write_num(success);
    write_str("/");
    write_num(N);
    write_str(" children succeeded ===\n");
    return fail > 0 ? 1 : 0;
}
