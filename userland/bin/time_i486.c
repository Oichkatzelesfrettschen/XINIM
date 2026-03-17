#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>

static void write_elapsed(unsigned long ms) {
    char buf[20]; int pos = 0;
    unsigned long s = ms / 1000, frac = ms % 1000;
    if (s == 0) buf[pos++] = '0';
    else { unsigned long v = s; while (v > 0) { buf[pos++] = (char)('0' + v % 10); v /= 10; }
           for (int i = 0; i < pos/2; ++i) { char t=buf[i]; buf[i]=buf[pos-1-i]; buf[pos-1-i]=t; } }
    write(2, buf, pos);
    write(2, ".", 1);
    char fb[3]; fb[0] = (char)('0' + frac / 100); fb[1] = (char)('0' + (frac / 10) % 10); fb[2] = (char)('0' + frac % 10);
    write(2, fb, 3);
}

int main(int argc, char **argv) {
    if (argc < 2) { write(2, "usage: time command [args]\n", 26); return 1; }
    struct timeval start, end;
    gettimeofday(&start, 0);
    int pid = fork();
    if (pid == 0) { execve(argv[1], argv + 1, 0); _exit(127); }
    int status = 0;
    waitpid(pid, &status, 0);
    gettimeofday(&end, 0);
    unsigned long ms = (unsigned long)(end.tv_sec - start.tv_sec) * 1000 +
                       (unsigned long)(end.tv_usec - start.tv_usec) / 1000;
    write(2, "\nreal\t0m", 8); write_elapsed(ms); write(2, "s\n", 2);
    write(2, "user\t0m0.000s\n", 14);
    write(2, "sys\t0m0.000s\n", 13);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
