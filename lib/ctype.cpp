/*=== MINIX MODERNIZATION HEADER BEGIN ===
   This file is part of a work in progress to reproduce the original MINIX simplicity on modern
armv7/arm64, i386-i686/x86_64, and risc-v 32/64 using C++23. Targeting arm64/x86_64 first.
=== MINIX MODERNIZATION HEADER END ===*/

#include "../include/ctype.hpppp"

char _ctype_[] = {
    0,  _C, _C,      _C,      _C,      _C,      _C,      _C,      _C, _C, _S, _S, _S, _S, _S, _C,
    _C, _C, _C,      _C,      _C,      _C,      _C,      _C,      _C, _C, _C, _C, _C, _C, _C, _C,
    _C, _S, _P,      _P,      _P,      _P,      _P,      _P,      _P, _P, _P, _P, _P, _P, _P, _P,
    _P, _N, _N,      _N,      _N,      _N,      _N,      _N,      _N, _N, _N, _P, _P, _P, _P, _P,
    _P, _P, _U | _X, _U | _X, _U | _X, _U | _X, _U | _X, _U | _X, _U, _U, _U, _U, _U, _U, _U, _U,
    _U, _U, _U,      _U,      _U,      _U,      _U,      _U,      _U, _U, _U, _U, _P, _P, _P, _P,
    _P, _P, _L | _X, _L | _X, _L | _X, _L | _X, _L | _X, _L | _X, _L, _L, _L, _L, _L, _L, _L, _L,
    _L, _L, _L,      _L,      _L,      _L,      _L,      _L,      _L, _L, _L, _L, _P, _P, _P, _P,
    _C};
