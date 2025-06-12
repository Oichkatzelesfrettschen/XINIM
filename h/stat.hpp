/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#pragma once

// Shared stat structure used by the kernel. Definitions live in the
// common header to avoid duplication.
#include "../include/shared/stat_struct.hpp"
#include "../h/type.hpp" // For mask_bits (uint16_t)

/* Some common definitions. */
// Converted to inline constexpr mask_bits for type safety
inline constexpr mask_bits S_IFMT = 0170000;  /* type of file */
inline constexpr mask_bits S_IFDIR = 0040000; /* directory */
inline constexpr mask_bits S_IFCHR = 0020000; /* character special */
inline constexpr mask_bits S_IFBLK = 0060000; /* block special */
inline constexpr mask_bits S_IFREG = 0100000; /* regular */
inline constexpr mask_bits S_ISUID = 04000;   /* set user id on execution */
inline constexpr mask_bits S_ISGID = 02000;   /* set group id on execution */
inline constexpr mask_bits S_ISVTX = 01000;   /* save swapped text even after use */
inline constexpr mask_bits S_IREAD = 00400;   /* read permission, owner */
inline constexpr mask_bits S_IWRITE = 00200;  /* write permission, owner */
inline constexpr mask_bits S_IEXEC = 00100;   /* execute/search permission, owner */
