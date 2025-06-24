/**
 * @file abort.cpp
 * @brief Implementation of the xinim_abort wrapper.
 */

#include "../include/xinim/abort.h"

extern "C" [[noreturn]] void xinim_abort() noexcept { std::exit(99); }
