/**
 * @file test_heap_allocator.cpp
 * @brief Host-side unit tests for the free-list kernel heap allocator.
 *
 * Tests: alloc/free cycles, coalescing, exhaustion, alignment,
 * double-free safety, fragmentation stress, and stats accuracy.
 */

#include "heap.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>

using namespace xinim::kernel;

// Each test uses its own heap arena to avoid cross-contamination.
static constexpr uint64_t ARENA_SIZE = 4096;

static void test_basic_alloc_free() {
    alignas(16) uint8_t arena[ARENA_SIZE];
    heap_init(arena, ARENA_SIZE);

    void* p = heap_alloc(64);
    assert(p != nullptr);

    // Pointer must be 16-byte aligned
    assert((reinterpret_cast<uintptr_t>(p) % 16) == 0);

    // Write pattern, verify no crash
    memset(p, 0xAA, 64);

    heap_free(p);

    // After free, should be able to allocate again
    void* p2 = heap_alloc(64);
    assert(p2 != nullptr);
    heap_free(p2);
}

static void test_multiple_alloc_free() {
    alignas(16) uint8_t arena[ARENA_SIZE];
    heap_init(arena, ARENA_SIZE);

    void* a = heap_alloc(32);
    void* b = heap_alloc(32);
    void* c = heap_alloc(32);
    assert(a && b && c);
    assert(a != b && b != c);

    heap_free(b);  // Free middle block
    heap_free(a);  // Free first block
    heap_free(c);  // Free last block

    // All memory returned -- should be able to allocate large block
    void* big = heap_alloc(128);
    assert(big != nullptr);
    heap_free(big);
}

static void test_coalescing_forward() {
    alignas(16) uint8_t arena[ARENA_SIZE];
    heap_init(arena, ARENA_SIZE);

    void* a = heap_alloc(64);
    void* b = heap_alloc(64);
    assert(a && b);

    // Free b then a -- coalescing should merge them
    heap_free(b);
    heap_free(a);

    // The coalesced block should be large enough for both + header
    auto stats = heap_stats();
    // Should have exactly 1 free block after full coalesce
    assert(stats.free_count == 1);
}

static void test_coalescing_backward() {
    alignas(16) uint8_t arena[ARENA_SIZE];
    heap_init(arena, ARENA_SIZE);

    void* a = heap_alloc(64);
    void* b = heap_alloc(64);
    void* c = heap_alloc(64);
    assert(a && b && c);

    // Free a, then b -- b should coalesce backward with a
    heap_free(a);
    heap_free(b);

    auto stats = heap_stats();
    // a+b coalesced into one free block, c is allocated, remainder is free
    assert(stats.free_count == 2);  // coalesced(a+b) + remainder

    heap_free(c);
    stats = heap_stats();
    assert(stats.free_count == 1);  // everything coalesced
}

static void test_exhaustion() {
    alignas(16) uint8_t arena[256];
    heap_init(arena, 256);

    // Exhaust the heap
    void* ptrs[16];
    int count = 0;
    while (count < 16) {
        void* p = heap_alloc(16);
        if (!p) break;
        ptrs[count++] = p;
    }
    assert(count > 0);

    // Further allocation must return nullptr
    assert(heap_alloc(16) == nullptr);

    // Free everything
    for (int i = 0; i < count; i++) {
        heap_free(ptrs[i]);
    }
}

static void test_alignment() {
    alignas(16) uint8_t arena[ARENA_SIZE];
    heap_init(arena, ARENA_SIZE);

    for (uint64_t sz = 1; sz <= 128; sz++) {
        void* p = heap_alloc(sz);
        assert(p != nullptr);
        assert((reinterpret_cast<uintptr_t>(p) % 16) == 0);
        heap_free(p);
    }
}

static void test_double_free() {
    alignas(16) uint8_t arena[ARENA_SIZE];
    heap_init(arena, ARENA_SIZE);

    void* p = heap_alloc(64);
    assert(p != nullptr);

    heap_free(p);
    // Double free should be a no-op (block already marked free)
    heap_free(p);

    auto stats = heap_stats();
    // Heap should still be consistent (1 free block)
    assert(stats.free_count == 1);
}

static void test_null_free() {
    // free(nullptr) must be a no-op
    heap_free(nullptr);
}

static void test_zero_alloc() {
    alignas(16) uint8_t arena[ARENA_SIZE];
    heap_init(arena, ARENA_SIZE);

    void* p = heap_alloc(0);
    assert(p == nullptr);
}

static void test_stats() {
    alignas(16) uint8_t arena[ARENA_SIZE];
    heap_init(arena, ARENA_SIZE);

    auto s0 = heap_stats();
    assert(s0.block_count == 1);
    assert(s0.free_count == 1);
    assert(s0.used_bytes == 0);

    void* a = heap_alloc(64);
    assert(a);

    auto s1 = heap_stats();
    assert(s1.used_bytes == 64);  // payload only
    assert(s1.free_count >= 1);   // remainder block

    heap_free(a);

    auto s2 = heap_stats();
    assert(s2.used_bytes == 0);
    assert(s2.free_count == 1);  // coalesced back
}

static void test_fragmentation_stress() {
    alignas(16) uint8_t arena[8192];
    heap_init(arena, 8192);

    // Allocate many small blocks
    constexpr int N = 64;
    void* ptrs[N];
    for (int i = 0; i < N; i++) {
        ptrs[i] = heap_alloc(32);
        if (!ptrs[i]) break;
    }

    // Free every other block (creates fragmentation)
    for (int i = 0; i < N; i += 2) {
        if (ptrs[i]) heap_free(ptrs[i]);
    }

    // Allocate into the freed slots
    for (int i = 0; i < N; i += 2) {
        void* p = heap_alloc(32);
        if (p) ptrs[i] = p;
    }

    // Free everything
    for (int i = 0; i < N; i++) {
        if (ptrs[i]) heap_free(ptrs[i]);
    }

    auto stats = heap_stats();
    // After freeing everything, should coalesce to 1 block
    assert(stats.free_count == 1);
    assert(stats.used_bytes == 0);
}

int main() {
    test_basic_alloc_free();
    test_multiple_alloc_free();
    test_coalescing_forward();
    test_coalescing_backward();
    test_exhaustion();
    test_alignment();
    test_double_free();
    test_null_free();
    test_zero_alloc();
    test_stats();
    test_fragmentation_stress();
    return 0;
}
