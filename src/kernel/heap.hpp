#pragma once
/**
 * @file heap.hpp
 * @brief Free-list kernel heap allocator.
 *
 * WHY: The v1.1.0 bump allocator has a no-op free(), leaking all memory.
 *      This free-list allocator supports genuine allocation and deallocation
 *      with forward coalescing, 16-byte alignment, and O(n) first-fit search.
 *
 * WHAT: Replaces the static kernel_heap[1MB] bump allocator in klib64.cpp
 *       and the duplicate kmalloc() in server_spawn.cpp with a single unified
 *       4MB heap.
 *
 * HOW: First-fit free-list with block splitting and forward coalescing.
 *      Each block has a 32-byte header (size, next, prev, free flag).
 *      All returned pointers are 16-byte aligned.
 */

#include <cstddef>
#include <cstdint>

namespace xinim::kernel {

/**
 * @brief Per-block header for the free-list allocator.
 *
 * Stored immediately before the user-visible pointer.
 * Size: 32 bytes (fits 16-byte alignment naturally).
 */
struct HeapBlock {
    uint64_t size;       ///< Usable payload size (excludes header)
    HeapBlock* next;     ///< Next block in address order
    HeapBlock* prev;     ///< Previous block in address order
    bool free;           ///< True if this block is available
    uint8_t pad_[7];     ///< Padding to 32 bytes
};

static_assert(sizeof(HeapBlock) == 32, "HeapBlock must be 32 bytes");
static_assert(alignof(HeapBlock) <= 16, "HeapBlock alignment must fit 16-byte guarantee");

/**
 * @brief Heap usage statistics.
 */
struct HeapStats {
    uint64_t used_bytes;    ///< Total bytes allocated (payload only)
    uint64_t free_bytes;    ///< Total bytes available (payload only)
    uint64_t largest_free;  ///< Largest contiguous free block (payload)
    uint32_t block_count;   ///< Total number of blocks (free + used)
    uint32_t free_count;    ///< Number of free blocks
};

/**
 * @brief Initialize the kernel heap.
 *
 * Must be called once before any allocation. Sets up a single free block
 * spanning the entire region.
 *
 * @param base  Start of heap memory (must be 16-byte aligned)
 * @param size  Total heap size in bytes (minimum 64 bytes)
 */
void heap_init(void* base, uint64_t size);

/**
 * @brief Allocate memory from the kernel heap.
 *
 * First-fit search. Splits oversized blocks. Returns 16-byte aligned pointer.
 *
 * @param size  Number of bytes to allocate (0 returns nullptr)
 * @return Pointer to allocated memory, or nullptr on exhaustion
 */
void* heap_alloc(uint64_t size);

/**
 * @brief Free previously allocated memory.
 *
 * Marks block as free and coalesces with adjacent free blocks.
 * Passing nullptr is a no-op.
 *
 * @param ptr  Pointer returned by heap_alloc (or nullptr)
 */
void heap_free(void* ptr);

/**
 * @brief Query heap usage statistics.
 *
 * Walks the block list and computes usage data.
 *
 * @return HeapStats structure with current heap state
 */
HeapStats heap_stats();

/**
 * @brief Convenience: total bytes currently allocated.
 */
uint64_t heap_used();

/**
 * @brief Convenience: total heap capacity.
 */
uint64_t heap_total();

} // namespace xinim::kernel
