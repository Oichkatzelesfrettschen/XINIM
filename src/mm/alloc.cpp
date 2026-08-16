/**
 * @file alloc.cpp
 * @brief Bounded physical-memory hole allocator for the memory manager.
 *
 * A dense fixed-capacity descriptor table keeps free physical ranges ordered
 * without relying on a hosted allocator. Allocation is first-fit. Freeing
 * coalesces adjacent ranges before consuming another descriptor.
 */

#include "alloc.hpp"
#ifdef XINIM_ARCH_X86_64
#include "../kernel/scoped_irq_lock.hpp"
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

    struct PhysicalExtent {
        uint64_t base;
        uint64_t length;
    };

    PhysicalExtent hole_table[PHYSICAL_HOLE_DESCRIPTOR_CAPACITY]{};
    PhysicalExtent allocation_table[PHYSICAL_ALLOCATION_DESCRIPTOR_CAPACITY]{};
    std::size_t hole_count = 0;
    std::size_t allocation_count = 0;
    alignas(64) std::atomic_flag allocator_lock = ATOMIC_FLAG_INIT;

    class PhysicalAllocatorGuard {
    public:
        PhysicalAllocatorGuard() noexcept {
            while (allocator_lock.test_and_set(std::memory_order_acquire)) {
#if defined(__x86_64__) || defined(__i386__)
                __builtin_ia32_pause();
#else
                std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
            }
        }

        ~PhysicalAllocatorGuard() { allocator_lock.clear(std::memory_order_release); }

        PhysicalAllocatorGuard(const PhysicalAllocatorGuard &) = delete;
        PhysicalAllocatorGuard &operator=(const PhysicalAllocatorGuard &) = delete;

    private:
#ifdef XINIM_ARCH_X86_64
        xinim::kernel::ScopedIrqLock irq_lock_{};
#endif
    };

    void remove_hole(std::size_t index) noexcept {
        for (std::size_t move_index = index + 1U; move_index < hole_count; ++move_index) {
            hole_table[move_index - 1U] = hole_table[move_index];
        }
        --hole_count;
    }

    void remove_allocation(std::size_t index) noexcept {
        for (std::size_t move_index = index + 1U; move_index < allocation_count; ++move_index) {
            allocation_table[move_index - 1U] = allocation_table[move_index];
        }
        --allocation_count;
    }

    [[nodiscard]] bool record_allocation(uint64_t base, uint64_t clicks) noexcept {
        const uint64_t allocation_end = base + clicks;
        std::size_t previous_index = allocation_count;
        std::size_t next_index = allocation_count;
        for (std::size_t index = 0U; index < allocation_count; ++index) {
            const PhysicalExtent &allocation = allocation_table[index];
            if (allocation.base + allocation.length == base) {
                previous_index = index;
            }
            if (allocation_end == allocation.base) {
                next_index = index;
            }
        }

        if (previous_index != allocation_count && next_index != allocation_count) {
            allocation_table[previous_index].length += clicks + allocation_table[next_index].length;
            remove_allocation(next_index);
            return true;
        }
        if (previous_index != allocation_count) {
            allocation_table[previous_index].length += clicks;
            return true;
        }
        if (next_index != allocation_count) {
            allocation_table[next_index].base = base;
            allocation_table[next_index].length += clicks;
            return true;
        }
        if (allocation_count == PHYSICAL_ALLOCATION_DESCRIPTOR_CAPACITY) {
            return false;
        }
        allocation_table[allocation_count] = PhysicalExtent{base, clicks};
        ++allocation_count;
        return true;
    }

    [[nodiscard]] bool insert_hole(uint64_t base, uint64_t clicks) noexcept {
        const uint64_t free_end = base + clicks;
        std::size_t insertion_index = 0U;
        while (insertion_index < hole_count && hole_table[insertion_index].base < base) {
            ++insertion_index;
        }

        const bool has_previous = insertion_index != 0U;
        const bool has_next = insertion_index != hole_count;
        if (has_previous) {
            const PhysicalExtent &previous = hole_table[insertion_index - 1U];
            if (previous.base + previous.length > base) {
                return false;
            }
        }
        if (has_next && free_end > hole_table[insertion_index].base) {
            return false;
        }

        const bool joins_previous =
            has_previous &&
            hole_table[insertion_index - 1U].base + hole_table[insertion_index - 1U].length == base;
        const bool joins_next = has_next && free_end == hole_table[insertion_index].base;

        if (joins_previous && joins_next) {
            PhysicalExtent &previous = hole_table[insertion_index - 1U];
            previous.length += clicks + hole_table[insertion_index].length;
            remove_hole(insertion_index);
            return true;
        }
        if (joins_previous) {
            hole_table[insertion_index - 1U].length += clicks;
            return true;
        }
        if (joins_next) {
            PhysicalExtent &next = hole_table[insertion_index];
            next.base = base;
            next.length += clicks;
            return true;
        }
        if (hole_count == PHYSICAL_HOLE_DESCRIPTOR_CAPACITY) {
            return false;
        }

        for (std::size_t move_index = hole_count; move_index > insertion_index; --move_index) {
            hole_table[move_index] = hole_table[move_index - 1U];
        }
        hole_table[insertion_index] = PhysicalExtent{base, clicks};
        ++hole_count;
        return true;
    }

    void reset_allocator() noexcept {
        hole_count = 0U;
        allocation_count = 0U;
    }

} // namespace

[[nodiscard]] uint64_t alloc_mem(uint64_t clicks) noexcept {
    const PhysicalAllocatorGuard guard{};
    if (clicks == 0U) {
        return NO_MEM;
    }

    for (std::size_t index = 0; index < hole_count; ++index) {
        PhysicalExtent &hole = hole_table[index];
        if (hole.length < clicks) {
            continue;
        }

        const uint64_t allocated_base = hole.base;
        if (!record_allocation(allocated_base, clicks)) {
            continue;
        }
        hole.base += clicks;
        hole.length -= clicks;
        if (hole.length == 0U) {
            remove_hole(index);
        }
        return allocated_base;
    }

    return NO_MEM;
}

[[nodiscard]] bool free_mem(uint64_t base, uint64_t clicks) noexcept {
    const PhysicalAllocatorGuard guard{};
    if (base == NO_MEM || clicks == 0U || base > std::numeric_limits<uint64_t>::max() - clicks) {
        return false;
    }

    const uint64_t free_end = base + clicks;
    std::size_t allocation_index = 0U;
    while (allocation_index < allocation_count) {
        const PhysicalExtent &allocation = allocation_table[allocation_index];
        if (allocation.base <= base && free_end <= allocation.base + allocation.length) {
            break;
        }
        ++allocation_index;
    }
    if (allocation_index == allocation_count) {
        return false;
    }

    const PhysicalExtent allocation = allocation_table[allocation_index];
    const uint64_t allocation_end = allocation.base + allocation.length;
    const bool splits_allocation = allocation.base < base && free_end < allocation_end;
    if ((splits_allocation && allocation_count == PHYSICAL_ALLOCATION_DESCRIPTOR_CAPACITY) ||
        !insert_hole(base, clicks)) {
        return false;
    }

    if (allocation.base == base && allocation_end == free_end) {
        remove_allocation(allocation_index);
    } else if (allocation.base == base) {
        allocation_table[allocation_index] = PhysicalExtent{free_end, allocation_end - free_end};
    } else if (allocation_end == free_end) {
        allocation_table[allocation_index].length = base - allocation.base;
    } else {
        allocation_table[allocation_index].length = base - allocation.base;
        allocation_table[allocation_count] = PhysicalExtent{free_end, allocation_end - free_end};
        ++allocation_count;
    }
    return true;
}

[[nodiscard]] uint64_t max_hole() noexcept {
    const PhysicalAllocatorGuard guard{};
    uint64_t largest_length = 0;
    for (std::size_t index = 0; index < hole_count; ++index) {
        if (hole_table[index].length > largest_length) {
            largest_length = hole_table[index].length;
        }
    }
    return largest_length;
}

void mem_init(uint64_t clicks) noexcept {
    const PhysicalAllocatorGuard guard{};
    reset_allocator();

    if (clicks <= 1U) {
        return;
    }

    hole_table[0] = PhysicalExtent{1U, clicks - 1U};
    hole_count = 1U;
}

[[nodiscard]] bool mem_init_from_memory_map(const xinim::boot::MemRange *ranges,
                                            std::size_t range_count) noexcept {
    const PhysicalAllocatorGuard guard{};
    reset_allocator();
    if (ranges == nullptr || range_count == 0U ||
        range_count > xinim::boot::kBootMemoryRangeCapacity) {
        return false;
    }

    for (std::size_t index = 0U; index < range_count; ++index) {
        const xinim::boot::MemRange &range = ranges[index];
        if (range.length != 0U &&
            range.base > std::numeric_limits<uint64_t>::max() - range.length) {
            reset_allocator();
            return false;
        }
    }

    for (std::size_t left_index = 0U; left_index < range_count; ++left_index) {
        const xinim::boot::MemRange &left = ranges[left_index];
        if (left.length == 0U) {
            continue;
        }
        const uint64_t left_end = left.base + left.length;
        for (std::size_t right_index = left_index + 1U; right_index < range_count; ++right_index) {
            const xinim::boot::MemRange &right = ranges[right_index];
            if (right.length == 0U) {
                continue;
            }
            const uint64_t right_end = right.base + right.length;
            if (left.base < right_end && right.base < left_end) {
                reset_allocator();
                return false;
            }
        }
    }

    for (std::size_t index = 0U; index < range_count; ++index) {
        const xinim::boot::MemRange &range = ranges[index];
        if (range.type != xinim::boot::MEMORY_RANGE_USABLE || range.length == 0U) {
            continue;
        }

        const uint64_t range_end = range.base + range.length;
        uint64_t first_click = range.base >> CLICK_SHIFT;
        if ((range.base & (CLICK_SIZE - 1U)) != 0U) {
            ++first_click;
        }
        if (first_click == NO_MEM) {
            first_click = 1U;
        }
        const uint64_t end_click = range_end >> CLICK_SHIFT;
        if (end_click <= first_click) {
            continue;
        }
        if (!insert_hole(first_click, end_click - first_click)) {
            reset_allocator();
            return false;
        }
    }

    if (hole_count == 0U) {
        reset_allocator();
        return false;
    }
    return true;
}
