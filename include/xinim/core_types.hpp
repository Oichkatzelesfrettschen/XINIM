#ifndef XINIM_CORE_TYPES_HPP
#define XINIM_CORE_TYPES_HPP

#include <cstddef> // For std::size_t, std::ptrdiff_t
#include <cstdint> // For fixed-width integer types

namespace xinim {

// Memory Address Types
using phys_addr_t = std::uint64_t; // Physical memory address
using virt_addr_t = std::uint64_t; // Virtual memory address

// Byte count types associated with physical/virtual address spaces
using phys_bytes_t = phys_addr_t;
using virt_bytes_t = virt_addr_t;

// Process, User, and Group Identifiers
using pid_t = std::int32_t; // Process ID
using uid_t = std::int32_t; // User ID (Minix historically used uint16_t, modernizing to int32_t)
using gid_t = std::int32_t; // Group ID (Minix historically used uint16_t, modernizing to int32_t)

// Filesystem-related Types
using dev_t = std::uint32_t;   // Device number
using ino_t = std::uint64_t;   // Inode number
using mode_t = std::uint32_t;  // File mode (permissions, type)
using nlink_t = std::uint32_t; // Link count
using off_t = std::int64_t;    // File offset or size

// Time-related Types
using time_t = std::int64_t; // Time in seconds (typically since epoch)

// Size and Count Types
using ssize_t = std::ptrdiff_t; // Signed size/count (can represent -1 for errors)
// std::size_t is used directly for unsigned sizes.

// Hardware-specific types
namespace hw {
// I/O port address type, typically 16-bit on x86
using port_t = std::uint16_t;

// DMA address type. This is often physical but can have constraints
// (e.g., 24-bit for ISA, 32-bit for older PCI, 64-bit for modern systems).
// Using uintptr_t and recommending static_asserts in hardware-specific code.
// Alternatively, a fixed size like uint32_t or uint64_t might be chosen
// based on dominant hardware targets. For now, uintptr_t is flexible.
using dma_addr_t = std::uintptr_t;
} // namespace hw

// Common XINIM status/error code type (placeholder, might be replaced by std::error_code or
// similar)
using status_t = int;
constexpr status_t OK = 0;

// Define common pointer types if needed, e.g.:
// using void_ptr = void*;
// using char_ptr = char*;

// Define NIL_PTR for broader compatibility if still used, though nullptr is preferred
#define NIL_PTR nullptr

} // namespace xinim

#endif // XINIM_CORE_TYPES_HPP
