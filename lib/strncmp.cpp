/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

int strncmp(s1, s2, n)
register char *s1, *s2;
int n;
{
/* Compare two strings, but at most n characters. */

  while (1) {
	if (*s1 != *s2) return(*s1 - *s2);
	if (*s1 == 0 || --n == 0) return(0);
	s1++;
	s2++;
  }
}
