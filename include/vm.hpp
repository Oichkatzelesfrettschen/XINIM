/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

#ifndef VM_H
#define VM_H

#include "../h/const.hpp"
#include "paging.hpp"
#include <array>
#include <cstdint>
#include <span>

/**
 * @brief Flags describing permissions and properties for a virtual memory region.
 */
enum class VmFlags : int {
    VM_READ = 0x1,    /**< Region is readable. */
    VM_WRITE = 0x2,   /**< Region is writable. */
    VM_EXEC = 0x4,    /**< Region is executable. */
    VM_PRIVATE = 0x8, /**< Region is private. */
    VM_SHARED = 0x10, /**< Region is shared. */
    VM_ANON = 0x20    /**< Region is anonymous. */
};

/// Enable bitwise OR for VmFlags.
inline constexpr VmFlags operator|(VmFlags l, VmFlags r) {
    return static_cast<VmFlags>(static_cast<int>(l) | static_cast<int>(r));
}
/// Enable bitwise AND for VmFlags.
inline constexpr VmFlags operator&(VmFlags l, VmFlags r) {
    return static_cast<VmFlags>(static_cast<int>(l) & static_cast<int>(r));
}

inline VmFlags &operator|=(VmFlags &l, VmFlags r) {
    l = l | r;
    return l;
}
inline VmFlags &operator&=(VmFlags &l, VmFlags r) {
    l = l & r;
    return l;
}

/// Maximum number of areas tracked for a process.
inline constexpr int VM_MAX_AREAS = 16;

/**
 * @brief Categories of memory areas managed by the VM subsystem.
 */
enum class VmAreaType : std::uint8_t {
    Unspecified, /**< Area type not specified. */
    Mapped,      /**< Memory region established via mmap or similar. */
    Stack,       /**< Stack region growing downward. */
    Heap,        /**< Dynamically allocated heap region. */
};

/**
 * @brief Contiguous virtual memory area owned by a process.
 */
struct vm_area {
    virt_addr64 start; ///< Inclusive start address.
    virt_addr64 end;   ///< Exclusive end address.
    VmFlags flags;     ///< Protection flags.
    VmAreaType type;   ///< Semantic type of this area.
};

/**
 * @brief Per-process bookkeeping of virtual memory areas.
 */
struct vm_proc {
    std::array<vm_area, VM_MAX_AREAS> areas{}; ///< Storage for owned areas.
    std::size_t area_count{};                  ///< Number of valid entries.

    /// Provide a span covering the valid areas.
    [[nodiscard]] std::span<vm_area> area_span() noexcept { return {areas.data(), area_count}; }
    /// Provide a read-only span covering the valid areas.
    [[nodiscard]] std::span<const vm_area> area_span() const noexcept {
        return {areas.data(), area_count};
    }
};

void vm_init() noexcept;
void *vm_alloc(u64_t bytes, VmFlags flags) noexcept;
void vm_handle_fault(int proc, virt_addr64 addr) noexcept;
int vm_fork(int parent, int child) noexcept;
void *vm_mmap(int proc, void *addr, u64_t length, VmFlags flags) noexcept;

#endif /* VM_H */
