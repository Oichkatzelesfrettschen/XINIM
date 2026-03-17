#include <unistd.h>
#include <string.h>

extern char **environ;

int main(void) {
    if (environ == 0) return 0;
    for (char **ep = environ; *ep != 0; ++ep) {
        write(1, *ep, strlen(*ep));
        write(1, "\n", 1);
    }
    return 0;
}
