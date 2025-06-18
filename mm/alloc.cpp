/**
 * @file alloc.cpp
 * @brief Physical memory allocator for the memory manager.
 *
 * This module keeps a list of free regions referred to as "holes" managed with
 * `std::list`. Memory is allocated in units of clicks using a first-fit
 * strategy. During system initialisation the regions occupied by the kernel and
 * memory manager are removed from the list so they cannot be reused.
 */

#include "../h/const.hpp"
#include "../h/type.hpp" // This should define phys_clicks as uint64_t
#include "const.hpp"     // For NO_MEM
#include <cstddef>       // For nullptr
#include <cstdint>       // For explicit uint64_t usage
#include <iterator>      // For std::begin/std::end
#include <list>          // For std::list

/** Maximum number of entries initially reserved. */
constexpr int NR_HOLES = 128;

/**
 * @brief Descriptor for a free region of physical memory.
 */
struct Hole {
    uint64_t base; ///< Start address of the hole in clicks.
    uint64_t len;  ///< Length of the hole in clicks.
};

/// List of available holes managed using RAII.
static std::list<Hole> hole_list;
/**
 * @brief Allocate a block from the hole list using first fit.
 *
 * @param clicks Number of clicks to allocate.
 * @return Base click address of the allocated block or ::NO_MEM.
 */
[[nodiscard]] uint64_t alloc_mem(uint64_t clicks) noexcept {
    for (auto it = hole_list.begin(); it != hole_list.end(); ++it) {
        if (it->len >= clicks) {
            uint64_t old_base = it->base;
            it->base += clicks;
            it->len -= clicks;
            if (it->len == 0) {
                hole_list.erase(it);
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
    auto it = hole_list.begin();
    while (it != hole_list.end() && it->base < base) {
        ++it;
    }
    auto inserted = hole_list.insert(it, new_hole);
    merge(inserted);
}

/**
 * @brief Merge a hole with adjacent holes if they are contiguous.
 *
 * @param it Iterator pointing to the hole to merge.
 */
static void merge(std::list<Hole>::iterator it) noexcept {
    if (it == hole_list.end()) {
        return;
    }
    auto next = std::next(it);
    if (next != hole_list.end() && it->base + it->len == next->base) {
        it->len += next->len;
        hole_list.erase(next);
    }
    if (it != hole_list.begin()) {
        auto prev = std::prev(it);
        if (prev->base + prev->len == it->base) {
            prev->len += it->len;
            hole_list.erase(it);
        }
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
 * The hole table is prepared so that one hole spans the entire region
 * described by @p clicks. All remaining table slots are added to the
 * free-slot list for later use.
 *
 * @param clicks Number of clicks available.
 */
void mem_init(uint64_t clicks) noexcept {
    hole_list.clear();
    hole_list.push_back({0, clicks});
}
