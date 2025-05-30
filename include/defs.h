#ifndef DEFS_H
#define DEFS_H

/* Custom fixed-width type definitions replacing <stdint.h>. */

/* The compiler must provide builtin fixed width integer types. */
#if defined(__clang__) || defined(__GNUC__)

typedef __INT8_TYPE__   i8_t;   /* signed 8  bit integer */
typedef __UINT8_TYPE__  u8_t;   /* unsigned 8  bit integer */
typedef __INT16_TYPE__  i16_t;  /* signed 16 bit integer */
typedef __UINT16_TYPE__ u16_t;  /* unsigned 16 bit integer */
typedef __INT32_TYPE__  i32_t;  /* signed 32 bit integer */
typedef __UINT32_TYPE__ u32_t;  /* unsigned 32 bit integer */
typedef __INT64_TYPE__  i64_t;  /* signed 64 bit integer */
typedef __UINT64_TYPE__ u64_t;  /* unsigned 64 bit integer */
typedef __UINTPTR_TYPE__ uptr_t; /* unsigned pointer sized integer */

/* Attribute helpers usable in headers and source files. */
#define PACKED __attribute__((packed)) /* structure packing */
#define NAKED  __attribute__((naked))  /* naked function */

/* Macros for defining 64-bit constants without littering the code with
 * long long suffixes. */
#define U64_C(val) val##ULL
#define I64_C(val) val##LL

#else
#error "Unsupported compiler"
#endif

#endif /* DEFS_H */
