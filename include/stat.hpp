/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

#pragma once

// Shared stat structure used by user space. The layout is provided by the
// common header to keep it consistent with the kernel version.
#include "shared/stat_struct.hpp" // This now includes xinim::core_types.hpp
#include <xinim/core_types.hpp>   // Ensure xinim::mode_t is directly available

/* Some common definitions. */
// Converted to inline constexpr xinim::mode_t for type safety
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
