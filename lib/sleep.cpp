#include "../include/lib.hpp" // C++17 header
#include "../include/signal.hpp"

PRIVATE void alfun(int signum) { (void)signum; }
PUBLIC void sleep(int n) {
    /* sleep(n) pauses for 'n' seconds by scheduling an alarm interrupt. */
    signal(SIGALRM, alfun);
    alarm(n);
    pause();
}
