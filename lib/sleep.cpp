/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "../include/lib.hpp" // C++17 header
#include "../include/signal.h"

static void alfun(int signum) { (void)signum; }
void sleep(int n) {
    /* sleep(n) pauses for 'n' seconds by scheduling an alarm interrupt. */
    signal(SIGALRM, alfun);
    alarm(n);
    pause();
}
