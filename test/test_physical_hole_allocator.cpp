/**
 * @file test_physical_hole_allocator.cpp
 * @brief Contract tests for the bounded physical-memory hole allocator.
 */

#include "alloc.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <thread>
#include <vector>

namespace {

    int failures = 0;

    void check(bool condition, const char *expression, int line) {
        if (!condition) {
            std::fprintf(stderr, "FAIL:%d: %s\n", line, expression);
            ++failures;
        }
    }

#define CHECK(expression) check((expression), #expression, __LINE__)

    static void test_initialization_and_first_fit() {
        mem_init(101U);
        CHECK(max_hole() == 100U);
        CHECK(alloc_mem(0U) == NO_MEM);

        const uint64_t first = alloc_mem(10U);
        const uint64_t second = alloc_mem(20U);
        const uint64_t third = alloc_mem(10U);
        CHECK(first == 1U);
        CHECK(second == 11U);
        CHECK(third == 31U);

        CHECK(free_mem(second, 20U));
        CHECK(alloc_mem(15U) == second);
        CHECK(max_hole() == 60U);
    }

    static void test_bidirectional_coalescing() {
        mem_init(41U);
        const uint64_t first = alloc_mem(10U);
        const uint64_t second = alloc_mem(10U);
        const uint64_t third = alloc_mem(10U);

        CHECK(first == 1U);
        CHECK(second == 11U);
        CHECK(third == 21U);
        CHECK(free_mem(second, 10U));
        CHECK(free_mem(first, 10U));
        CHECK(free_mem(third, 10U));
        CHECK(max_hole() == 40U);
        CHECK(alloc_mem(40U) == 1U);
        CHECK(max_hole() == 0U);
    }

    static void test_invalid_free_rejection() {
        mem_init(21U);
        const uint64_t allocated = alloc_mem(10U);
        CHECK(allocated == 1U);

        CHECK(!free_mem(0U, 1U));
        CHECK(!free_mem(allocated, 0U));
        CHECK(!free_mem(std::numeric_limits<uint64_t>::max(), 2U));
        CHECK(!free_mem(20U, 2U));
        CHECK(!free_mem(allocated + 1U, 10U));
        CHECK(!free_mem(allocated, 11U));
        CHECK(free_mem(allocated, 10U));
        CHECK(!free_mem(allocated, 10U));
        CHECK(max_hole() == 20U);
    }

    static void test_descriptor_exhaustion_is_recoverable() {
        constexpr uint64_t managed_clicks =
            static_cast<uint64_t>(2U * PHYSICAL_HOLE_DESCRIPTOR_CAPACITY + 2U);
        mem_init(managed_clicks);

        for (uint64_t index = 0; index < managed_clicks - 1U; ++index) {
            CHECK(alloc_mem(1U) == index + 1U);
        }
        CHECK(alloc_mem(1U) == NO_MEM);

        for (std::size_t index = 0; index < PHYSICAL_HOLE_DESCRIPTOR_CAPACITY; ++index) {
            CHECK(free_mem(static_cast<uint64_t>(2U * index + 1U), 1U));
        }
        CHECK(!free_mem(managed_clicks - 1U, 1U));

        CHECK(alloc_mem(1U) == 1U);
        CHECK(free_mem(managed_clicks - 1U, 1U));
    }

    static void test_adjacent_allocations_share_ownership_extent() {
        constexpr uint64_t managed_clicks =
            static_cast<uint64_t>(PHYSICAL_ALLOCATION_DESCRIPTOR_CAPACITY + 2U);
        mem_init(managed_clicks);

        for (std::size_t index = 0U; index < PHYSICAL_ALLOCATION_DESCRIPTOR_CAPACITY + 1U;
             ++index) {
            CHECK(alloc_mem(1U) == static_cast<uint64_t>(index + 1U));
        }
        CHECK(alloc_mem(1U) == NO_MEM);
        CHECK(free_mem(1U, managed_clicks - 1U));
        CHECK(alloc_mem(managed_clicks - 1U) == 1U);
    }

    static void test_partial_release_splits_and_rejoins_ownership() {
        mem_init(10U);
        for (uint64_t click = 1U; click <= 6U; ++click) {
            CHECK(alloc_mem(1U) == click);
        }
        CHECK(free_mem(3U, 2U));
        CHECK(!free_mem(3U, 2U));
        CHECK(alloc_mem(2U) == 3U);
        CHECK(free_mem(1U, 6U));
        CHECK(max_hole() == 9U);
    }

    static void test_boot_memory_map_admission() {
        constexpr uint64_t page = CLICK_SIZE;
        const xinim::boot::MemRange ranges[] = {
            {0U, page, xinim::boot::MEMORY_RANGE_RESERVED},
            {page, 3U * page, xinim::boot::MEMORY_RANGE_USABLE},
            {4U * page, page, xinim::boot::MEMORY_RANGE_KERNEL_AND_MODULES},
            {5U * page, 2U * page, xinim::boot::MEMORY_RANGE_USABLE},
        };

        CHECK(mem_init_from_memory_map(ranges, 4U));
        CHECK(max_hole() == 3U);
        CHECK(alloc_mem(3U) == 1U);
        CHECK(alloc_mem(2U) == 5U);
        CHECK(alloc_mem(1U) == NO_MEM);

        const xinim::boot::MemRange overlapping[] = {
            {page, 3U * page, xinim::boot::MEMORY_RANGE_USABLE},
            {2U * page, 3U * page, xinim::boot::MEMORY_RANGE_USABLE},
        };
        CHECK(!mem_init_from_memory_map(overlapping, 2U));
        CHECK(max_hole() == 0U);

        const xinim::boot::MemRange reserved_overlap[] = {
            {page, 3U * page, xinim::boot::MEMORY_RANGE_USABLE},
            {2U * page, page, xinim::boot::MEMORY_RANGE_RESERVED},
        };
        CHECK(!mem_init_from_memory_map(reserved_overlap, 2U));
        CHECK(max_hole() == 0U);

        const xinim::boot::MemRange overflowing[] = {
            {std::numeric_limits<uint64_t>::max() - page + 1U, page,
             xinim::boot::MEMORY_RANGE_USABLE},
        };
        CHECK(!mem_init_from_memory_map(overflowing, 1U));

        const xinim::boot::MemRange reserved_overflowing[] = {
            {std::numeric_limits<uint64_t>::max(), 2U, xinim::boot::MEMORY_RANGE_RESERVED},
        };
        CHECK(!mem_init_from_memory_map(reserved_overflowing, 1U));
        CHECK(!mem_init_from_memory_map(nullptr, 0U));
    }

    static void test_boot_memory_map_boundaries() {
        constexpr uint64_t page = CLICK_SIZE;
        const xinim::boot::MemRange begins_at_zero[] = {
            {0U, 4U * page, xinim::boot::MEMORY_RANGE_USABLE},
        };
        CHECK(mem_init_from_memory_map(begins_at_zero, 1U));
        CHECK(alloc_mem(3U) == 1U);
        CHECK(alloc_mem(1U) == NO_MEM);

        const xinim::boot::MemRange unsorted_adjacent[] = {
            {5U * page, 2U * page, xinim::boot::MEMORY_RANGE_USABLE},
            {page, 4U * page, xinim::boot::MEMORY_RANGE_USABLE},
        };
        CHECK(mem_init_from_memory_map(unsorted_adjacent, 2U));
        CHECK(max_hole() == 6U);
        CHECK(alloc_mem(6U) == 1U);

        const xinim::boot::MemRange sub_click[] = {
            {page + 1U, 3U * page - 2U, xinim::boot::MEMORY_RANGE_USABLE},
        };
        CHECK(mem_init_from_memory_map(sub_click, 1U));
        CHECK(max_hole() == 1U);
        CHECK(alloc_mem(1U) == 2U);

        const xinim::boot::MemRange maximum_end[] = {
            {std::numeric_limits<uint64_t>::max() - 2U * page + 1U, 2U * page - 2U,
             xinim::boot::MEMORY_RANGE_USABLE},
        };
        CHECK(mem_init_from_memory_map(maximum_end, 1U));
        CHECK(max_hole() == 1U);
    }

    static void test_boot_memory_map_capacity_contract() {
        constexpr uint64_t page = CLICK_SIZE;
        std::array<xinim::boot::MemRange, xinim::boot::kBootMemoryRangeCapacity> exact{};
        for (std::size_t index = 0U; index < exact.size(); ++index) {
            exact[index] = {
                static_cast<uint64_t>(2U * index + 1U) * page,
                page,
                xinim::boot::MEMORY_RANGE_USABLE,
            };
        }
        CHECK(mem_init_from_memory_map(exact.data(), exact.size()));
        CHECK(max_hole() == 1U);

        std::array<xinim::boot::MemRange, xinim::boot::kBootMemoryRangeCapacity + 1U> excessive{};
        for (std::size_t index = 0U; index < excessive.size(); ++index) {
            excessive[index] = {
                static_cast<uint64_t>(2U * index + 1U) * page,
                page,
                xinim::boot::MEMORY_RANGE_USABLE,
            };
        }
        CHECK(!mem_init_from_memory_map(excessive.data(), excessive.size()));
        CHECK(max_hole() == 0U);
    }

    static void test_partial_release_transaction_boundaries() {
        mem_init(21U);
        CHECK(alloc_mem(10U) == 1U);
        CHECK(free_mem(1U, 2U));
        CHECK(alloc_mem(2U) == 1U);
        CHECK(free_mem(9U, 2U));
        CHECK(alloc_mem(2U) == 9U);
        CHECK(free_mem(4U, 2U));
        CHECK(!free_mem(3U, 2U));
        CHECK(alloc_mem(2U) == 4U);
        CHECK(free_mem(1U, 10U));
        CHECK(max_hole() == 20U);
    }

    static void test_smp_serialization() {
        constexpr std::size_t thread_count = 8U;
        constexpr std::size_t allocations_per_thread = 32U;
        constexpr std::size_t iterations = 20U;
        mem_init(4096U);

        std::atomic<bool> start{false};
        std::atomic<std::size_t> concurrent_failures{0U};
        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        for (std::size_t thread_index = 0U; thread_index < thread_count; ++thread_index) {
            threads.emplace_back([&start, &concurrent_failures]() {
                std::array<uint64_t, allocations_per_thread> allocations{};
                while (!start.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                for (std::size_t iteration = 0U; iteration < iterations; ++iteration) {
                    for (uint64_t &allocation : allocations) {
                        allocation = alloc_mem(1U);
                        if (allocation == NO_MEM) {
                            concurrent_failures.fetch_add(1U, std::memory_order_relaxed);
                        }
                    }
                    for (uint64_t allocation : allocations) {
                        if (allocation != NO_MEM && !free_mem(allocation, 1U)) {
                            concurrent_failures.fetch_add(1U, std::memory_order_relaxed);
                        }
                    }
                }
            });
        }
        start.store(true, std::memory_order_release);
        for (std::thread &thread : threads) {
            thread.join();
        }
        CHECK(concurrent_failures.load(std::memory_order_relaxed) == 0U);
        CHECK(max_hole() == 4095U);
    }

} // namespace

int main() {
    test_initialization_and_first_fit();
    test_bidirectional_coalescing();
    test_invalid_free_rejection();
    test_descriptor_exhaustion_is_recoverable();
    test_adjacent_allocations_share_ownership_extent();
    test_partial_release_splits_and_rejoins_ownership();
    test_boot_memory_map_admission();
    test_boot_memory_map_boundaries();
    test_boot_memory_map_capacity_contract();
    test_partial_release_transaction_boundaries();
    test_smp_serialization();
    return failures == 0 ? 0 : 1;
}
