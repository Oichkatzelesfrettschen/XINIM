#include "../include/lib.h"
#include "../include/signal.h"

PRIVATE void alfun(int signum) { (void)signum; }
PUBLIC void sleep(int n)
{
/* sleep(n) pauses for 'n' seconds by scheduling an alarm interrupt. */
  signal(SIGALRM, alfun);
  alarm(n);
  pause();
}
