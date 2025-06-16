#include "../include/lib.hpp" // C++23 header
#include "../include/signal.h"

static void alfun(int signum) { (void)signum; }
void sleep(int n) {
    /* sleep(n) pauses for 'n' seconds by scheduling an alarm interrupt. */
    signal(SIGALRM, alfun);
    alarm(n);
    pause();
}
