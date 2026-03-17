#include <unistd.h>
#include <signal.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) { write(2, "usage: nohup command [args]\n", 28); return 127; }
    signal(1, (void(*)(int))1); /* SIG_IGN for SIGHUP */
    execve(argv[1], argv + 1, 0);
    write(2, "nohup: exec failed\n", 19);
    return 127;
}
