/*
 * Compare two strings and return the difference between
 * the first differing characters or zero if they match.
 */
int strcmp(s1, s2)
register char *s1, *s2;
{
/* Compare 2 strings. */

  while (1) {
	if (*s1 != *s2) return(*s1 - *s2);
	if (*s1 == 0) return(0);
	s1++;
	s2++;
  }
}
