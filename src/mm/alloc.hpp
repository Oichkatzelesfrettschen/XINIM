#pragma once
/**
 * @file alloc.hpp
 * @brief Interfaces for memory allocation used by the memory manager.
 *
 * The allocator maintains a free list of hole descriptors that track
 * unallocated physical regions. Addresses and lengths are expressed in
 * clicks, a machine-dependent unit equal to @c CLICK_SIZE bytes, ensuring
 * that all allocations are page aligned.
 *
 * @ingroup memory
 */

#include "const.hpp"

#include <cstddef>
#include <cstdint>
#include <xinim/boot/bootinfo.hpp>

/**
 * Each admitted boot range receives sixteen free-extent slots. This bounded
 * fragmentation policy is derived from the boot-map intake contract rather
 * than the unrelated scheduler process count.
 */
inline constexpr std::size_t PHYSICAL_HOLE_DESCRIPTOR_CAPACITY =
    16U * xinim::boot::kBootMemoryRangeCapacity;

/**
 * In each admitted physical run, allocated and free extents alternate, so the
 * number of allocated extents cannot exceed the number of free extents plus
 * the number of admitted runs.
 */
inline constexpr std::size_t PHYSICAL_ALLOCATION_DESCRIPTOR_CAPACITY =
    PHYSICAL_HOLE_DESCRIPTOR_CAPACITY + xinim::boot::kBootMemoryRangeCapacity;

static_assert(PHYSICAL_HOLE_DESCRIPTOR_CAPACITY == 1024U);
static_assert(PHYSICAL_ALLOCATION_DESCRIPTOR_CAPACITY == 1088U);

/**
 * @brief Allocate a block of physical memory measured in clicks.
 *
 * The allocator uses a first-fit policy on a list of free holes. Ownership of
 * the reserved region is transferred to the caller, who must release it with
 * free_mem(). The base and size are aligned to @c CLICK_SIZE bytes.
 *
 * @param clicks Number of memory clicks to allocate.
 * A zero-sized request fails. Physical click zero remains reserved because
 * ::NO_MEM uses zero as the failure sentinel.
 *
 * @return Base click address of the allocated block or ::NO_MEM on failure.
 * Calls are serialized by an interrupt-safe SMP lock.
 *
 * @ingroup memory
 */
[[nodiscard]] uint64_t alloc_mem(uint64_t clicks) noexcept;

/**
 * @brief Free a previously allocated block of physical memory.
 *
 * The caller relinquishes ownership of the range back to the allocator. The
 * range must be wholly contained in a live allocated extent. Partial release
 * splits or trims that extent while preserving ownership of retained clicks.
 *
 * Invalid, overlapping, overflowing, and out-of-range blocks are rejected.
 * A middle split is also rejected when the bounded allocation descriptor table
 * is full. The caller retains ownership after every rejected release.
 *
 * @param base   Starting click of the block to free.
 * @param clicks Size of the block in clicks.
 * @return True when ownership was accepted; false when the block was rejected.
 * Calls are serialized by an interrupt-safe SMP lock.
 *
 * @ingroup memory
 */
[[nodiscard]] bool free_mem(uint64_t base, uint64_t clicks) noexcept;

/**
 * @brief Return the size of the largest available hole.
 *
 * @return Length in clicks of the largest free region.
 *
 * @ingroup memory
 */
[[nodiscard]] uint64_t max_hole() noexcept;

/**
 * @brief Initialise the hole allocator with a single region of memory.
 *
 * The allocator assumes ownership of the entire region and manages it via the
 * internal free list of hole descriptors. Subsequent allocations carve out
 * subranges while preserving page alignment.
 *
 * Click zero is excluded from the managed region so successful allocations
 * cannot collide with the ::NO_MEM sentinel.
 *
 * @param clicks End-exclusive physical click limit.
 *
 * @ingroup memory
 */
void mem_init(uint64_t clicks) noexcept;

/**
 * @brief Initialise free holes from normalized bootloader memory ranges.
 *
 * Only complete clicks in ranges tagged ::xinim::boot::MEMORY_RANGE_USABLE are
 * admitted. Reserved, kernel, module, framebuffer, ACPI, and bad-memory ranges
 * remain unavailable.
 *
 * @param ranges Normalized physical memory ranges.
 * @param range_count Number of entries in @p ranges.
 * @return True when the complete map was admitted; false for invalid input or
 * descriptor exhaustion. Failure leaves the allocator empty.
 */
[[nodiscard]] bool mem_init_from_memory_map(const xinim::boot::MemRange *ranges,
                                            std::size_t range_count) noexcept;
