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

#include <cstdint>

/**
 * @brief Allocate a block of physical memory measured in clicks.
 *
 * The allocator uses a first-fit policy on a list of free holes. Ownership of
 * the reserved region is transferred to the caller, who must release it with
 * free_mem(). The base and size are aligned to @c CLICK_SIZE bytes.
 *
 * @param clicks Number of memory clicks to allocate.
 * @return Base click address of the allocated block or ::NO_MEM on failure.
 *
 * @ingroup memory
 */
[[nodiscard]] uint64_t alloc_mem(uint64_t clicks) noexcept;

/**
 * @brief Free a previously allocated block of physical memory.
 *
 * The caller relinquishes ownership of the region back to the allocator. The
 * block must have been obtained from alloc_mem() and respect the original
 * click alignment.
 *
 * @param base   Starting click of the block to free.
 * @param clicks Size of the block in clicks.
 *
 * @ingroup memory
 */
void free_mem(uint64_t base, uint64_t clicks) noexcept;

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
 * @param clicks Total number of clicks available.
 *
 * @ingroup memory
 */
void mem_init(uint64_t clicks) noexcept;