/**
 * @file chmem.cpp
 * @brief Set total memory size for execution in a legacy executable.
 * @author Andy Tanenbaum (original author)
 * @date 2023-10-27 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the original `chmem` utility from MINIX.
 * It modifies the header of an executable file to change the total memory allocated
 * for the stack and data segments. The program ensures type safety, resource management
 * via RAII, and robust error handling through exceptions.
 *
 * Usage: chmem {=|+|-}amount file
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <filesystem>
#include <system_error>
#include <cstdint>
#include <stdexcept>

// Define the legacy executable format details in a dedicated namespace.
namespace ExecutableFormat {
    // Using a struct to represent the header provides type safety and clarity.
    struct Header {
        uint32_t magic;
        uint32_t flags;
        uint32_t textSize;
        uint32_t dataSize;
        uint32_t bssSize;
        uint32_t entryPoint; // Assuming, based on standard a.out
        uint32_t totalAllocation;
        uint32_t symbolSize; // Assuming
    };

    constexpr uint16_t MAGIC = 0x0301;       // Magic number for executable programs
    constexpr uint32_t SEP_ID_BIT = 0x0020;  // Bit for separate I/D space
    constexpr int64_t MAX_ALLOCATION = 65535L; // Max allocation size for stack+data
}

/**
 * @class MemoryPatcher
 * @brief Manages the modification of an executable's memory allocation.
 *
 * This class encapsulates the logic for reading the executable header,
 * calculating the new memory allocation, and writing the changes back to the file.
 * It uses RAII for file handling and throws exceptions on errors.
 */
class MemoryPatcher {
public:
    /**
     * @brief Constructs a MemoryPatcher for a given file.
     * @param filePath The path to the executable file.
     * @throws std::runtime_error if the file cannot be opened.
     */
    MemoryPatcher(const std::filesystem::path& filePath);

    /**
     * @brief Patches the memory allocation in the executable header.
     * @param op The operation to perform ('=', '+'
