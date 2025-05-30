/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

extern char _ctype_[]; /* global character classification table */

/* Bit masks for character properties. */
enum CtypeMask : unsigned char {
    _U = 0001, /* upper case */
    _L = 0002, /* lower case */
    _N = 0004, /* numeric digit */
    _S = 0010, /* whitespace */
    _P = 0020, /* punctuation */
    _C = 0040, /* control */
    _X = 0100  /* hexadecimal digit */
};

/* Character classification helpers implemented as inline functions. */
inline bool isalpha(int c) { return (_ctype_ + 1)[c] & (_U | _L); }
inline bool isupper(int c) { return (_ctype_ + 1)[c] & _U; }
inline bool islower(int c) { return (_ctype_ + 1)[c] & _L; }
inline bool isdigit(int c) { return (_ctype_ + 1)[c] & _N; }
inline bool isxdigit(int c) { return (_ctype_ + 1)[c] & (_N | _X); }
inline bool isspace(int c) { return (_ctype_ + 1)[c] & _S; }
inline bool ispunct(int c) { return (_ctype_ + 1)[c] & _P; }
inline bool isalnum(int c) { return (_ctype_ + 1)[c] & (_U | _L | _N); }
inline bool isprint(int c) { return (_ctype_ + 1)[c] & (_P | _U | _L | _N); }
inline bool iscntrl(int c) { return (_ctype_ + 1)[c] & _C; }
inline bool isascii(int c) { return static_cast<unsigned>(c) <= 0177; }
