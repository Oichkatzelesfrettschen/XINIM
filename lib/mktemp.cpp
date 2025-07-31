/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* mktemp - make a name for a temporary file */

char *mktemp(template)
char *template;
{
  int pid, k;
  char *p;

  pid = getpid();		/* get process id as semi-unique number */
  p = template;
  while (*p++) ;		/* find end of string */
  p--;				/* backup to last character */

  /* Replace XXXXXX at end of template with pid. */
  while (*--p == 'X') {
	*p = '0' + (pid % 10);
	pid = pid/10;
  }
  return(template);
}
