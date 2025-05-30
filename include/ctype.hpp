/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

extern char _ctype_[];

#define _U 0001
#define _L 0002
#define _N 0004
#define _S 0010
#define _P 0020
#define _C 0040
#define _X 0100

#define isalpha(c) ((_ctype_ + 1)[c] & (_U | _L))
#define isupper(c) ((_ctype_ + 1)[c] & _U)
#define islower(c) ((_ctype_ + 1)[c] & _L)
#define isdigit(c) ((_ctype_ + 1)[c] & _N)
#define isxdigit(c) ((_ctype_ + 1)[c] & (_N | _X))
#define isspace(c) ((_ctype_ + 1)[c] & _S)
#define ispunct(c) ((_ctype_ + 1)[c] & _P)
#define isalnum(c) ((_ctype_ + 1)[c] & (_U | _L | _N))
#define isprint(c) ((_ctype_ + 1)[c] & (_P | _U | _L | _N))
#define iscntrl(c) ((_ctype_ + 1)[c] & _C)
#define isascii(c) ((unsigned)(c) <= 0177)
