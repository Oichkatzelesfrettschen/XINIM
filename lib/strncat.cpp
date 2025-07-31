/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

char *strncat(s1, s2, n)
register char *s1, *s2;
int n;
{
/* Append s2 to the end of s1, but no more than n characters */

  char *original = s1;

  if (n == 0) return(s1);

  /* Find the end of s1. */
  while (*s1 != 0) s1++;

  /* Now copy s2 to the end of s1. */
  while (*s2 != 0) {
	*s1++ = *s2++;
	if (--n == 0) break;
  }
  *s1 = 0;
  return(original);
}
