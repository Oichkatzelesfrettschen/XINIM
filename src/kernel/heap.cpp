/**
 * @file heap.cpp
 * @brief Free-list kernel heap allocator implementation.
 *
 * First-fit with forward coalescing. See heap.hpp for design rationale.
 */

#include "heap.hpp"
#include <cstring>

namespace xinim::kernel {

// Module state -- initialized by heap_init().
static HeapBlock* heap_head_ = nullptr;
static uint64_t heap_capacity_ = 0;

void heap_init(void* base, uint64_t size) {
    if (!base || size < sizeof(HeapBlock) + 16) return;

    // Align base to 16 bytes (the header itself is 32 bytes, payload starts at +32)
    auto addr = reinterpret_cast<uint64_t>(base);
    uint64_t aligned = (addr + 15) & ~15ULL;
    uint64_t lost = aligned - addr;
    if (lost >= size) return;
    size -= lost;

    heap_capacity_ = size;
    heap_head_ = reinterpret_cast<HeapBlock*>(aligned);

    // Create one large free block spanning the entire region.
    heap_head_->size = size - sizeof(HeapBlock);
    heap_head_->next = nullptr;
    heap_head_->prev = nullptr;
    heap_head_->free = true;
    memset(heap_head_->pad_, 0, sizeof(heap_head_->pad_));
}

void* heap_alloc(uint64_t size) {
    if (size == 0 || !heap_head_) return nullptr;

    // Round up to 16-byte alignment
    size = (size + 15) & ~15ULL;

    // First-fit search
    HeapBlock* block = heap_head_;
    while (block) {
        if (block->free && block->size >= size) {
            // Found a fit -- try to split if there is room for another block + 16 bytes
            uint64_t remaining = block->size - size;
            if (remaining >= sizeof(HeapBlock) + 16) {
                // Split: create a new free block after the allocated region
                auto* new_block = reinterpret_cast<HeapBlock*>(
                    reinterpret_cast<uint8_t*>(block) + sizeof(HeapBlock) + size
                );
                new_block->size = remaining - sizeof(HeapBlock);
                new_block->next = block->next;
                new_block->prev = block;
                new_block->free = true;
                memset(new_block->pad_, 0, sizeof(new_block->pad_));

                if (block->next) {
                    block->next->prev = new_block;
                }
                block->next = new_block;
                block->size = size;
            }

            block->free = false;
            // Return pointer past the header
            return reinterpret_cast<void*>(
                reinterpret_cast<uint8_t*>(block) + sizeof(HeapBlock)
            );
        }
        block = block->next;
    }

    return nullptr; // Heap exhausted
}

void heap_free(void* ptr) {
    if (!ptr) return;

    // Recover the block header
    auto* block = reinterpret_cast<HeapBlock*>(
        reinterpret_cast<uint8_t*>(ptr) - sizeof(HeapBlock)
    );

    // Guard: already free (double-free)
    if (block->free) return;

    block->free = true;

    // Coalesce with next block if it is free
    if (block->next && block->next->free) {
        HeapBlock* absorbed = block->next;
        block->size += sizeof(HeapBlock) + absorbed->size;
        block->next = absorbed->next;
        if (absorbed->next) {
            absorbed->next->prev = block;
        }
    }

    // Coalesce with previous block if it is free
    if (block->prev && block->prev->free) {
        HeapBlock* absorber = block->prev;
        absorber->size += sizeof(HeapBlock) + block->size;
        absorber->next = block->next;
        if (block->next) {
            block->next->prev = absorber;
        }
    }
}

HeapStats heap_stats() {
    HeapStats stats{};
    HeapBlock* block = heap_head_;
    while (block) {
        stats.block_count++;
        if (block->free) {
            stats.free_count++;
            stats.free_bytes += block->size;
            if (block->size > stats.largest_free) {
                stats.largest_free = block->size;
            }
        } else {
            stats.used_bytes += block->size;
        }
        block = block->next;
    }
    return stats;
}

uint64_t heap_used() {
    return heap_stats().used_bytes;
}

uint64_t heap_total() {
    return heap_capacity_;
}

} // namespace xinim::kernel
