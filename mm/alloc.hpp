#pragma once
/**
 * @file alloc.hpp
 * @brief Interfaces for memory allocation used by the memory manager.
 */

#include <cstdint>

/**
 * @brief Allocate a block of physical memory measured in clicks.
 *
 * The allocator uses a first-fit policy on a `std::vector` of free holes.
 *
 * @param clicks Number of memory clicks to allocate.
 * @return Base click address of the allocated block or ::NO_MEM on failure.
 */
[[nodiscard]] uint64_t alloc_mem(uint64_t clicks) noexcept;

/**
 * @brief Free a previously allocated block of physical memory.
 *
 * @param base   Starting click of the block to free.
 * @param clicks Size of the block in clicks.
 */
void free_mem(uint64_t base, uint64_t clicks) noexcept;

/**
 * @brief Return the size of the largest available hole.
 */
[[nodiscard]] uint64_t max_hole() noexcept;

/**
 * @brief Initialise the hole allocator with a single region of memory.
 *
 * @param clicks Total number of clicks available.
 */
void mem_init(uint64_t clicks) noexcept;
