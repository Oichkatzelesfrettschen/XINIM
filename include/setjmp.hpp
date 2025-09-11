/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

#ifndef XINIM_SETJMP_H
#define XINIM_SETJMP_H

/*
 * Provide a thin wrapper around the C++ standard setjmp facilities so the
 * legacy sources can continue to include this header without change.  The
 * jmp_buf alias exposes the host representation from <csetjmp> while the
 * wrapper functions simply delegate to std::setjmp and std::longjmp.
 */

#include <csetjmp>

// Just use the standard library versions directly
using ::jmp_buf;
using ::setjmp;
using ::longjmp;

#endif /* XINIM_SETJMP_H */
