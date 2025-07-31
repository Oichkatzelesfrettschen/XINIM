/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "signal.hpp"
extern int errno;
int testnr;
int errct;

int zilch[5000];

main()
{
  int i;

  printf("Test  7 ");
  for (i = 0; i < 150; i++) {
	test70();
  }
  if (errct == 0)
	printf("ok\n");
  else
	printf("%d errors\n", errct);
}



test70()
{
  int i, err, pid;

  signal(SIGQUIT, SIG_IGN);
  err = 0;
  for (i=0; i<5000; i++) if (zilch[i] != 0) err++;
  if (err > 0) e(1);
  kill(getpid(), SIGQUIT);
}





e(n)
int n;
{
  printf("Subtest %d,  error %d  errno=%d  ", testnr, n, errno);
  perror("");
  errct++;
}
