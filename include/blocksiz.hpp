/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#pragma once

#include "defs.hpp" // For std::size_t (via xinim/core_types.hpp)

inline constexpr std::size_t BLOCK_SIZE = 1024; // File system data block size
