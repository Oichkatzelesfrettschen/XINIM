/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#define NULL  (char *) 0
char *getenv(name)
register char *name;
{
  extern char **environ;
  register char **v = environ, *p, *q;

  while ((p = *v) != NULL) {
	q = name;
	while (*p++ == *q)
		if (*q++ == 0)
			continue;
	if (*(p - 1) != '=')
		continue;
	return(p);
  }
  return(0);
}
