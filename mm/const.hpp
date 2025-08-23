#pragma once
// Modernized for C++23

#include "../../h/const.hpp" // For MAX_PATH, MAX_ISTACK_BYTES
#include "../../h/type.hpp"  // For phys_clicks
#include <cstddef>           // For std::size_t

/* Constants used by the Memory Manager. */

inline constexpr std::size_t ZEROBUF_SIZE = 1024; /* buffer size for erasing memory */

/* Size of MM's stack depends mostly on do_exec(). */
#if ZEROBUF_SIZE > MAX_PATH
inline constexpr std::size_t MM_STACK_BYTES = MAX_ISTACK_BYTES + ZEROBUF_SIZE + 384;
#else
inline constexpr std::size_t MM_STACK_BYTES = MAX_ISTACK_BYTES + MAX_PATH + 384;
#endif

inline constexpr std::size_t PAGE_SIZE = 4096;
inline constexpr std::size_t MAX_PAGES = 1048576;
inline constexpr std::size_t HDR_SIZE = 32;
inline constexpr phys_clicks NO_MEM = 0; /* returned by alloc_mem() when mem is up */

#ifndef __cplusplus
#define printf printk
#endif
