/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#pragma once

// Shared stat structure used by the kernel. Definitions live in the
// common header to avoid duplication.
#include "../include/shared/stat_struct.hpp" // Defines struct stat with xinim types
#include "../h/type.hpp" // For xinim::mode_t (via core_types.hpp) and other h/ specific types

/* Some common definitions. */
// Changed from mask_bits (uint16_t) to xinim::mode_t (std::uint32_t) for consistency
// with struct stat's st_mode.
inline constexpr xinim::mode_t S_IFMT = 0170000;  /* type of file */
inline constexpr xinim::mode_t S_IFDIR = 0040000; /* directory */
inline constexpr xinim::mode_t S_IFCHR = 0020000; /* character special */
inline constexpr xinim::mode_t S_IFBLK = 0060000; /* block special */
inline constexpr xinim::mode_t S_IFREG = 0100000; /* regular */
inline constexpr xinim::mode_t S_ISUID = 04000;   /* set user id on execution */
inline constexpr xinim::mode_t S_ISGID = 02000;   /* set group id on execution */
inline constexpr xinim::mode_t S_ISVTX = 01000;   /* save swapped text even after use */
inline constexpr xinim::mode_t S_IREAD = 00400;   /* read permission, owner */
inline constexpr xinim::mode_t S_IWRITE = 00200;  /* write permission, owner */
inline constexpr xinim::mode_t S_IEXEC = 00100;   /* execute/search permission, owner */