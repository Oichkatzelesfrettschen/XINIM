#ifndef XINIM_CORE_TYPES_HPP
#define XINIM_CORE_TYPES_HPP

/**
 * @file core_types.hpp
 * @brief Fundamental type aliases used throughout XINIM.
 */

#include <cstddef> /// For std::size_t, std::ptrdiff_t
#include <cstdint> /// For fixed-width integer types

namespace xinim {

/// Physical memory address type.
using phys_addr_t = std::uint64_t;
/// Virtual memory address type.
using virt_addr_t = std::uint64_t;

/// Byte count type associated with physical address space.
using phys_bytes_t = phys_addr_t;
/// Byte count type associated with virtual address space.
using virt_bytes_t = virt_addr_t;

/// Process identifier.
using pid_t = std::int32_t;
/// User identifier.
using uid_t = std::int32_t;
/// Group identifier.
using gid_t = std::int32_t;

/// Device number type.
using dev_t = std::uint32_t;
/// Inode number type.
using ino_t = std::uint64_t;
/// File mode (permissions, type).
using mode_t = std::uint32_t;
/// Link count type.
using nlink_t = std::uint32_t;
/// File offset or size type.
using off_t = std::int64_t;

/// Time in seconds since epoch.
using time_t = std::int64_t;

/// Signed size/count that can represent -1 for errors.
using ssize_t = std::ptrdiff_t;
/// Unsigned size type is provided by `std::size_t`.

/// Hardware-specific type aliases.
namespace hw {
/// I/O port address type, typically 16-bit on x86.
using port_t = std::uint16_t;
/// DMA address type used by hardware interfaces.
using dma_addr_t = std::uintptr_t;
} // namespace hw

/// Common XINIM status/error code type (placeholder, may be replaced).
using status_t = int;
/// Conventional success status code.
constexpr status_t OK = 0;

// Define common pointer types if needed, e.g.:
// using void_ptr = void*;
// using char_ptr = char*;

/// Compatibility macro mapping NIL_PTR to `nullptr`.
#define NIL_PTR nullptr

} // namespace xinim

#endif // XINIM_CORE_TYPES_HPP
