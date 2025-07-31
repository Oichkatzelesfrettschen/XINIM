/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* Integer to ASCII for signed decimal integers. */

static int next;
static char qbuf[8];

char *itoa(n)
int n;
{
  register int r, k;
  int flag = 0;

  next = 0;
  if (n < 0) {
	qbuf[next++] = '-';
	n = -n;
  }
  if (n == 0) {
	qbuf[next++] = '0';
  } else {
	k = 10000;
	while (k > 0) {
		r = n/k;
		if (flag || r > 0) {
			qbuf[next++] = '0' + r;
			flag = 1;
		}
		n -= r * k;
		k = k/10;
	}
  }
  qbuf[next] = 0;
  return(qbuf);
}
