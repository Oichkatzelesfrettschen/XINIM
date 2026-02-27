/**
 * @file kernel.cpp
 * @brief XINIM Kernel Implementation (Bare-metal refactored)
 */

#include <xinim/kernel/kernel.hpp>
#include "console.hpp"

namespace xinim {
namespace kernel {

Kernel::Kernel() : initialized_(false) {}

bool Kernel::initialize() {
    Console::printf("Kernel initialization starting...\n");
    initialized_ = true;
    Console::printf("Kernel initialization complete!\n");
    return true;
}

void Kernel::run() {
    if (!initialized_) return;
    Console::printf("XINIM Kernel running...\n");
}

void Kernel::shutdown() {
    Console::printf("Kernel shutting down...\n");
    initialized_ = false;
    Console::printf("Kernel shutdown complete!\n");
}

} // namespace kernel
} // namespace xinim

