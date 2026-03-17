/**
 * @file portio.hpp
 * @brief Backward-compatible x86_64 wrapper for shared x86 port I/O.
 */
#pragma once

#include <xinim/arch/x86/portio.hpp>

namespace xinim::arch::x86_64 {

using xinim::arch::x86::inb;
using xinim::arch::x86::inw;
using xinim::arch::x86::inl;
using xinim::arch::x86::outb;
using xinim::arch::x86::outw;
using xinim::arch::x86::outl;
using xinim::arch::x86::io_wait;

} // namespace xinim::arch::x86_64
