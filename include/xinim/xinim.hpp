/**
 * @file xinim.hpp
 * @brief Main XINIM header - includes all subsystems
 */

#ifndef XINIM_HPP
#define XINIM_HPP

// Core language and standard library
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <expected>
#include <span>
#include <concepts>
#include <ranges>

// XINIM subsystems
#include <xinim/kernel/kernel.hpp>
#include <xinim/hal/hal.hpp>
#include <xinim/crypto/crypto.hpp>
#include <xinim/fs/filesystem.hpp>
#include <xinim/mm/memory.hpp>
#include <xinim/net/network.hpp>

// Version information
#define XINIM_VERSION_MAJOR 1
#define XINIM_VERSION_MINOR 0
#define XINIM_VERSION_PATCH 0
#define XINIM_VERSION_STRING "1.0.0"

namespace xinim {

// Main system class
class System {
public:
    System();
    ~System();

    // Initialize all subsystems
    bool initialize();

    // Run the system
    int run();

    // Shutdown the system
    void shutdown();

private:
    bool initialized_;
};

} // namespace xinim

#endif // XINIM_HPP
