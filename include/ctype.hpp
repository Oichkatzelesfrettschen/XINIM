#ifndef LIBC_CTYPE_HPP
#define LIBC_CTYPE_HPP

extern char ctype_[]; /* global character classification table */

/* Bit masks for character properties. */
enum CtypeMask : unsigned char {
    U = 0001, /* upper case */
    L = 0002, /* lower case */
    N = 0004, /* numeric digit */
    S = 0010, /* whitespace */
    P = 0020, /* punctuation */
    C = 0040, /* control */
    X = 0100  /* hexadecimal digit */
};

/* Character classification helpers implemented as inline functions. */
inline bool isalpha(int c) noexcept { return (ctype_ + 1)[c] & (U | L); }
inline bool isupper(int c) noexcept { return (ctype_ + 1)[c] & U; }
inline bool islower(int c) noexcept { return (ctype_ + 1)[c] & L; }
inline bool isdigit(int c) noexcept { return (ctype_ + 1)[c] & N; }
inline bool isxdigit(int c) noexcept { return (ctype_ + 1)[c] & (N | X); }
inline bool isspace(int c) noexcept { return (ctype_ + 1)[c] & S; }
inline bool ispunct(int c) noexcept { return (ctype_ + 1)[c] & P; }
inline bool isalnum(int c) noexcept { return (ctype_ + 1)[c] & (U | L | N); }
inline bool isprint(int c) noexcept { return (ctype_ + 1)[c] & (P | U | L | N); }
inline bool iscntrl(int c) noexcept { return (ctype_ + 1)[c] & C; }
inline bool isascii(int c) noexcept { return static_cast<unsigned>(c) <= 0177; }

#endif // LIBC_CTYPE_H