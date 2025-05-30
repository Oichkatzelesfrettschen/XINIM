#ifndef DEFS_H
#define DEFS_H

/* Custom fixed-width type definitions replacing <stdint.h>. */
#if defined(__clang__) || defined(__GNUC__)

typedef __INT8_TYPE__ i8_t;
typedef __UINT8_TYPE__ u8_t;
typedef __INT16_TYPE__ i16_t;
typedef __UINT16_TYPE__ u16_t;
typedef __INT32_TYPE__ i32_t;
typedef __UINT32_TYPE__ u32_t;
typedef __INT64_TYPE__ i64_t;
typedef __UINT64_TYPE__ u64_t;
typedef __UINTPTR_TYPE__ uptr_t;

/* Macros for defining 64-bit constants without littering the code with
 * long long suffixes. */
#define U64_C(val) val##ULL
#define I64_C(val) val##LL

#else
#error "Unsupported compiler"
#endif

#endif /* DEFS_H */
