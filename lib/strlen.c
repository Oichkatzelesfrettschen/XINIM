/*
 * Compute the length of a string.
 */
int strlen(s)
char *s;
{
/* Return length of s. */

  char *original = s;

  while (*s != 0) s++;
  return(s - original);
}
