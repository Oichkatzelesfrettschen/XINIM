/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

#ifndef DEFS_H
#define DEFS_H

/* Custom fixed-width type definitions replacing <stdint.h>. */

/* The compiler must provide builtin fixed width integer types. */
#if defined(__clang__) || defined(__GNUC__)

using i8_t = __INT8_TYPE__;      /* signed 8  bit integer */
using u8_t = __UINT8_TYPE__;     /* unsigned 8  bit integer */
using i16_t = __INT16_TYPE__;    /* signed 16 bit integer */
using u16_t = __UINT16_TYPE__;   /* unsigned 16 bit integer */
using i32_t = __INT32_TYPE__;    /* signed 32 bit integer */
using u32_t = __UINT32_TYPE__;   /* unsigned 32 bit integer */
using i64_t = __INT64_TYPE__;    /* signed 64 bit integer */
using u64_t = __UINT64_TYPE__;   /* unsigned 64 bit integer */
using uptr_t = __UINTPTR_TYPE__; /* unsigned pointer sized integer */

/* Attribute helpers usable in headers and source files. */
#define PACKED __attribute__((packed)) /* structure packing */
#define NAKED __attribute__((naked))   /* naked function */

/* Macros for defining 64-bit constants without littering the code with
 * long long suffixes. */
#define U64_C(val) val##ULL
#define I64_C(val) val##LL

#else
#error "Unsupported compiler"
#endif

#endif /* DEFS_H */
