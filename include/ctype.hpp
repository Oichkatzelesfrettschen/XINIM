/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#pragma once

#include "../../include/xinim/core_types.hpp" // Added for consistency

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
// These functions should take int as per C standard, but ensure c is treated as unsigned char for array indexing.
// The (ctype_ + 1)[c] idiom implies c should be usable as an index after being offset.
// Standard ctype functions typically cast char to unsigned char before using it as an index.
// For this pass, assuming the existing (ctype_ + 1)[c] is safe for valid character inputs.

inline bool isalpha(int c) noexcept { return ((ctype_ + 1)[c] & (U | L)) != 0; }
inline bool isupper(int c) noexcept { return ((ctype_ + 1)[c] & U) != 0; }
inline bool islower(int c) noexcept { return ((ctype_ + 1)[c] & L) != 0; }
inline bool isdigit(int c) noexcept { return ((ctype_ + 1)[c] & N) != 0; }
inline bool isxdigit(int c) noexcept { return ((ctype_ + 1)[c] & (N | X)) != 0; }
inline bool isspace(int c) noexcept { return ((ctype_ + 1)[c] & S) != 0; }
inline bool ispunct(int c) noexcept { return ((ctype_ + 1)[c] & P) != 0; }
inline bool isalnum(int c) noexcept { return ((ctype_ + 1)[c] & (U | L | N)) != 0; }
inline bool isprint(int c) noexcept { return ((ctype_ + 1)[c] & (P | U | L | N | S)) != 0; } // Added S to isprint, common for space
inline bool iscntrl(int c) noexcept { return ((ctype_ + 1)[c] & C) != 0; }
inline bool isascii(int c) noexcept { return static_cast<unsigned>(c) <= 0177; }
// Adding isgraph as it's standard and related to isprint
inline bool isgraph(int c) noexcept { return ((ctype_ + 1)[c] & (P | U | L | N)) != 0; }


// Character conversion functions
inline int toupper(int c) noexcept {
    if (islower(c)) { // Relies on islower from this file
        return c - 'a' + 'A'; // Simple ASCII conversion
    }
    return c;
}

inline int tolower(int c) noexcept {
    if (isupper(c)) { // Relies on isupper from this file
        return c - 'A' + 'a'; // Simple ASCII conversion
    }
    return c;
}

// It's common for ctype.h to define _toupper and _tolower macros
// that expect valid lowercase/uppercase char, often for internal use by libc.
// For now, only providing standard toupper/tolower.

// Ensure EOF is handled correctly by users of these functions,
// as ctype functions are specified for `int` argument that is
// representable as `unsigned char` or `EOF`. The `(ctype_ + 1)[c]`
// indexing assumes `c` is not EOF and is within array bounds.
// This is an existing characteristic of the code.
