#include <sys/utsname.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    struct utsname u;
    if (uname(&u) != 0) {
        write(2, "uname: syscall failed\n", 22);
        return 1;
    }
    int show_all = 0, show_s = 0, show_n = 0, show_r = 0, show_m = 0;
    for (int i = 1; i < argc; ++i) {
        for (const char *p = argv[i]; *p; ++p) {
            if (*p == '-') continue;
            if (*p == 'a') show_all = 1;
            else if (*p == 's') show_s = 1;
            else if (*p == 'n') show_n = 1;
            else if (*p == 'r') show_r = 1;
            else if (*p == 'm') show_m = 1;
        }
    }
    if (!show_all && !show_s && !show_n && !show_r && !show_m) show_s = 1;
    if (show_all) { show_s = show_n = show_r = show_m = 1; }

    int first = 1;
    if (show_s) { if (!first) write(1, " ", 1); write(1, u.sysname, strlen(u.sysname)); first = 0; }
    if (show_n) { if (!first) write(1, " ", 1); write(1, u.nodename, strlen(u.nodename)); first = 0; }
    if (show_r) { if (!first) write(1, " ", 1); write(1, u.release, strlen(u.release)); first = 0; }
    if (show_m) { if (!first) write(1, " ", 1); write(1, u.machine, strlen(u.machine)); first = 0; }
    write(1, "\n", 1);
    return 0;
}
