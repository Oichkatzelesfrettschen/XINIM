#pragma once
#include <cstdint>
#include <cstddef>

namespace xinim {
    // Memory and addressing types
    using phys_bytes = std::uint64_t;  // Physical memory address
    using vir_bytes = std::size_t;     // Virtual memory address
    using phys_clicks = std::uint64_t; // Physical memory in clicks
    using vir_clicks = std::size_t;    // Virtual memory in clicks

    // Process and system types
    using pid_t = std::int32_t;        // Process ID
    using uid_t = std::int32_t;        // User ID
    using gid_t = std::int32_t;        // Group ID
    using dev_t = std::int32_t;        // Device number
    using ino_t = std::uint64_t;       // Inode number
    using mode_t = std::uint32_t;      // File mode
    using nlink_t = std::uint32_t;    // Link count
    using zone_t = std::uint32_t;      // Zone number
    using bit_t = std::uint32_t;       // Bit number
    using time_t = std::int64_t;       // Time value

    // Hardware-constrained types (DO NOT CHANGE SIZES)
    namespace hw {
        using dma_addr_t = std::uint32_t;  // DMA limited to 20 bits
        using port_t = std::uint16_t;      // I/O port address
        using irq_t = std::uint8_t;        // IRQ number
    }
}
