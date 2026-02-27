/**
 * @file filesystem.cpp
 * @brief Filesystem Initialization
 */

#include <xinim/fs/filesystem.hpp>
#include "console.hpp"

namespace xinim {
namespace fs {

bool initialize() {
    Console::printf("Filesystem initialization...\n");
    return true;
}

} // namespace fs
} // namespace xinim

