/**
 * @file hal.cpp
 * @brief Hardware Abstraction Layer Implementation (Bare-metal refactored)
 */

#include <xinim/hal/hal.hpp>
#include "console.hpp"

namespace xinim {
namespace hal {

bool initialize() {
    Console::printf("HAL initialization...\n");
    return true;
}

} // namespace hal
} // namespace xinim

