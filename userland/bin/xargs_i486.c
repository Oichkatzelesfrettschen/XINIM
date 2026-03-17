#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    const char *cmd = "/bin/echo";
    int max_args = 128;
    int argi = 1;

    while (argi < argc && argv[argi][0] == '-') {
        if (argv[argi][1] == 'n' && argi + 1 < argc) {
            long v = 0;
            for (const char *p = argv[++argi]; *p >= '0' && *p <= '9'; ++p)
                v = v * 10 + (*p - '0');
            if (v > 0) max_args = (int)v;
        }
        ++argi;
    }

    if (argi < argc) cmd = argv[argi++];

    /* Read stdin, split on whitespace, accumulate args, exec when full */
    char buf[4096];
    char *args[130];
    int nargs = 0;
    ssize_t buf_len = 0;
    ssize_t n;
    int status = 0;

    args[nargs++] = (char *)cmd;

    while ((n = read(0, buf + buf_len, sizeof(buf) - 1 - buf_len)) > 0) {
        buf_len += n;
        buf[buf_len] = '\0';

        char *p = buf;
        while (*p) {
            /* Skip whitespace */
            while (*p == ' ' || *p == '\t' || *p == '\n') ++p;
            if (*p == '\0') break;

            /* Extract word */
            char *word = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n') ++p;
            if (*p) *p++ = '\0';

            args[nargs++] = word;
            if (nargs >= max_args + 1 || nargs >= 129) {
                args[nargs] = 0;
                int pid = fork();
                if (pid == 0) {
                    execve(cmd, args, 0);
                    _exit(127);
                }
                if (pid > 0) {
                    int ws;
                    waitpid(pid, &ws, 0);
                    if (ws != 0) status = 1;
                }
                nargs = 1;
            }
        }
        /* Move remaining to start */
        buf_len = 0;
    }

    /* Flush remaining args */
    if (nargs > 1) {
        args[nargs] = 0;
        int pid = fork();
        if (pid == 0) {
            execve(cmd, args, 0);
            _exit(127);
        }
        if (pid > 0) {
            int ws;
            waitpid(pid, &ws, 0);
            if (ws != 0) status = 1;
        }
    }
    return status;
}
