/**
 * @file alloc.cpp
 * @brief Physical memory allocator for the memory manager.
 *
 * This module keeps a list of free regions referred to as "holes" managed with
 * `std::vector`.
 * Memory is allocated in units of clicks using a first-fit
 * strategy. During system
 * initialisation the regions occupied by the kernel and memory manager are removed from the list so
 * they cannot be reused.
 */

#include "../h/const.hpp"
#include "../h/type.hpp" // This should define phys_clicks as uint64_t
#include "const.hpp"     // For NO_MEM
#include <algorithm>     // For std::lower_bound
#include <cstddef>       // For nullptr
#include <cstdint>       // For explicit uint64_t usage
#include <iterator>      // For std::begin/std::end
#include <vector>        // For std::vector

/**
 * @brief Descriptor for a free region of physical memory.
 */
struct Hole {
    uint64_t base; ///< Start address of the hole in clicks.
    uint64_t len;  ///< Length of the hole in clicks.
};

/// List of available holes managed with automatic storage.
static std::vector<Hole> hole_list;
/**
 * @brief Allocate a block from the hole list using first fit.
 *
 * @param clicks Number of clicks to allocate.
 * @return Base click address of the allocated block or ::NO_MEM.
 */
[[nodiscard]] uint64_t alloc_mem(uint64_t clicks) noexcept {
    for (std::size_t i = 0; i < hole_list.size(); ++i) {
        auto &hole = hole_list[i];
        if (hole.len >= clicks) {
            const uint64_t old_base = hole.base;
            hole.base += clicks;
            hole.len -= clicks;
            if (hole.len == 0) {
                hole_list.erase(hole_list.begin() + static_cast<ptrdiff_t>(i));
            }
            return old_base;
        }
    }
    return NO_MEM;
}
/**
 * @brief Return a block of memory to the allocator.
 *
 * @param base   Starting click of the block.
 * @param clicks Size of the block in clicks.
 */
void free_mem(uint64_t base, uint64_t clicks) noexcept {
    Hole new_hole{base, clicks};
    auto it = std::lower_bound(hole_list.begin(), hole_list.end(), base,
                               [](const Hole &h, uint64_t value) { return h.base < value; });
    auto idx = static_cast<std::size_t>(it - hole_list.begin());
    hole_list.insert(it, new_hole);
    merge(idx);
}

/**
 * @brief Merge a hole with adjacent holes if they are contiguous.
 *
 * @param idx Index of the
 * hole to merge within @ref hole_list.
 */
static void merge(std::size_t idx) noexcept {
    if (idx >= hole_list.size()) {
        return;
    }
    if (idx + 1 < hole_list.size() &&
        hole_list[idx].base + hole_list[idx].len == hole_list[idx + 1].base) {
        hole_list[idx].len += hole_list[idx + 1].len;
        hole_list.erase(hole_list.begin() + static_cast<ptrdiff_t>(idx + 1));
    }
    if (idx > 0 && hole_list[idx - 1].base + hole_list[idx - 1].len == hole_list[idx].base) {
        hole_list[idx - 1].len += hole_list[idx].len;
        hole_list.erase(hole_list.begin() + static_cast<ptrdiff_t>(idx));
    }
}

/**
 * @brief Return the size of the largest available hole.
 */
[[nodiscard]] uint64_t max_hole() noexcept {
    uint64_t max = 0;
    for (const auto &h : hole_list) {
        if (h.len > max) {
            max = h.len;
        }
    }
    return max;
}

/**
 * @brief Initialise the hole allocator with a single region.
 *
 * The hole list is prepared so that one entry spans the entire region
 * described by @p clicks.
 * Subsequent allocations carve from this entry,
 * and additional holes are inserted as regions are
 * freed.
 *
 * @param clicks Number of clicks available.
 */
void mem_init(uint64_t clicks) noexcept {
    hole_list.clear();
    hole_list.push_back({0, clicks});
}
