/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

    /* library routine for copying structs with unpleasant alignment */

    __stb(n, f, t)
	    register char *f, *t; register int n;
    {
	    if (n > 0)
		    do
			    *t++ = *f++;
		    while (--n);
    }
