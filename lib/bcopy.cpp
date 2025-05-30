/* Copy a block of memory */
void bcopy(char *old, char *new, int n)
{
/* Copy a block of data. */

  while (n--) *new++ = *old++;
}
