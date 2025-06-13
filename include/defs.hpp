/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#ifndef DEFS_H
#define DEFS_H

#pragma once

#include "../xinim/core_types.hpp" // Provides standard fixed-width integers and other core types.

/* Attribute helpers usable in headers and source files. */
// These are GCC/Clang specific. For broader portability, consider alternatives
// or ensure they are only used in contexts where these compilers are assumed.
#define PACKED __attribute__((packed)) /* structure packing */
#define NAKED __attribute__((naked))   /* naked function */

/* Macros for defining 64-bit constants without littering the code with
 * long long suffixes. Standard C++ ULL/LL suffixes on literals are preferred
 * in new code, but these can remain for existing code compatibility. */
#define U64_C(val) val##ULL
#define I64_C(val) val##LL

// Common project-specific constants or further utility macros could go here if any.
// For example, EXTERN if it's defined as `extern` could be here,
// or TRUE/FALSE if not using `bool` directly everywhere.
// However, the original defs.hpp provided did not have these.
// `xinim::OK` and `nullptr` are available from core_types.hpp.

#endif /* DEFS_H */
