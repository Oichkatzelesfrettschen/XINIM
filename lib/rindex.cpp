/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

char *rindex(s, c)
register char *s, c;
{
  register char *result;

  result = 0;
  do
	if (*s == c)
		result = s;
  while (*s++ != 0);
  return(result);
}
