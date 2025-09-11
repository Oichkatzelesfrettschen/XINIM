/**
 * @file defs.hpp
 * @brief Fixed-width integer aliases and attribute helpers used across XINIM.
 */

#ifndef DEFS_H
#define DEFS_H

/// Custom fixed-width type definitions replacing `<cstdint>`.

/* The compiler must provide builtin fixed width integer types. */
#if defined(__clang__) || defined(__GNUC__)

/// Signed 8-bit integer type.
using i8_t = __INT8_TYPE__;
/// Unsigned 8-bit integer type.
using u8_t = __UINT8_TYPE__;
/// Signed 16-bit integer type.
using i16_t = __INT16_TYPE__;
/// Unsigned 16-bit integer type.
using u16_t = __UINT16_TYPE__;
/// Signed 32-bit integer type.
using i32_t = __INT32_TYPE__;
/// Unsigned 32-bit integer type.
using u32_t = __UINT32_TYPE__;
/// Signed 64-bit integer type.
using i64_t = __INT64_TYPE__;
/// Unsigned 64-bit integer type.
using u64_t = __UINT64_TYPE__;
/// Unsigned pointer-sized integer type.
using uptr_t = __UINTPTR_TYPE__;

/// Attribute helper for structure packing.
#define PACKED __attribute__((packed))
/// Attribute helper for naked functions.
#define NAKED __attribute__((naked))

/** Macro to build an unsigned 64-bit constant without a long long suffix. */
#define U64_C(val) val##ULL
/** Macro to build a signed 64-bit constant without a long long suffix. */
#define I64_C(val) val##LL

#else
#error "Unsupported compiler"
#endif

#endif /* DEFS_H */
