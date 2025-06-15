/**
 * @file alloc.cpp
 * @brief Physical memory allocator for the memory manager.
 *
 * This module keeps a linked list of free regions referred to as "holes".
 * Memory is allocated in units of clicks using a first-fit strategy.  During
 * system initialisation the regions occupied by the kernel and memory manager
 * are removed from the list so they cannot be reused.
 */

#include "../h/const.hpp"
#include "../h/type.hpp" // This should define phys_clicks as uint64_t
#include "const.hpp"     // For NO_MEM
#include <cstddef>       // For nullptr
#include <cstdint>       // For explicit uint64_t usage
#include <iterator>      // For std::begin/std::end

/** Maximum number of entries in the hole table. */
constexpr int NR_HOLES = 128;
// constexpr struct hole *NIL_HOLE = nullptr; // Definition removed

/**
 * @brief Descriptor for a free region of physical memory.
 */
PRIVATE struct hole {
    uint64_t h_base;     ///< Start address of the hole in clicks.
    uint64_t h_len;      ///< Length of the hole in clicks.
    struct hole *h_next; ///< Next hole in the linked list.
} hole[NR_HOLES];

/** Pointer to the first hole in the list. */
PRIVATE struct hole *hole_head;
/** Pointer to the list of unused table slots. */
PRIVATE struct hole *free_slots;
/**
 * @brief Allocate a block from the hole list using first fit.
 *
 * @param clicks Number of clicks to allocate.
 * @return Base click address of the allocated block or ::NO_MEM.
 */
[[nodiscard]] PUBLIC uint64_t alloc_mem(uint64_t clicks) noexcept { // phys_clicks -> uint64_t

    register struct hole *hp, *prev_ptr;
    uint64_t old_base; // phys_clicks -> uint64_t

    hp = hole_head;
    while (hp != nullptr) { // NIL_HOLE -> nullptr
        if (hp->h_len >= clicks) {
            /* We found a hole that is big enough.  Use it. */
            old_base = hp->h_base; /* remember where it started */
            hp->h_base += clicks;  /* bite a piece off */
            hp->h_len -= clicks;   /* ditto */

            /* If hole is only partly used, reduce size and return. */
            if (hp->h_len != 0)
                return (old_base);
            /**
             * @brief Remove an entry from the hole list.
             *
             * @param prev_ptr Hole preceding the one to remove.
             * @param hp       Hole to remove.
             */
            /* The entire hole has been used up.  Manipulate free list. */
            del_slot(prev_ptr, hp);
            return (old_base);
        }

        prev_ptr = hp;
        hp = hp->h_next;
    }
    return (NO_MEM);
}
/**
 * @brief Return a block of memory to the allocator.
 *
 * @param base   Starting click of the block.
 * @param clicks Size of the block in clicks.
 */
PUBLIC void free_mem(uint64_t base, uint64_t clicks) noexcept { // phys_clicks -> uint64_t
    register struct hole *hp, *new_ptr, *prev_ptr;

    if ((new_ptr = free_slots) == nullptr) // NIL_HOLE -> nullptr
        panic("Hole table full", NO_NUM);
    new_ptr->h_base = base;
    new_ptr->h_len = clicks;
    free_slots = new_ptr->h_next;
    hp = hole_head;

    /* If this block's address is numerically less than the lowest hole currently
     * available, or if no holes are currently available, put this hole on the
     * front of the hole list.
     */
    if (hp == nullptr || base <= hp->h_base) { // NIL_HOLE -> nullptr
        /* Block to be freed goes on front of hole list. */
        new_ptr->h_next = hp;
        hole_head = new_ptr;
        merge(new_ptr);
        return;
    }

    /* Block to be returned does not go on front of hole list. */
    while (hp != nullptr && base > hp->h_base) { // NIL_HOLE -> nullptr
        prev_ptr = hp;
        hp = hp->h_next;
    }

    /* We found where it goes.  Insert block after 'prev_ptr'. */
    new_ptr->h_next = prev_ptr->h_next;
    prev_ptr->h_next = new_ptr;
    merge(prev_ptr); /* sequence is 'prev_ptr', 'new_ptr', 'hp' */
}

/**
 * @brief Remove an entry from the hole list.
 *
 * @param prev_ptr Hole preceding the one to remove.
 * @param hp       Hole to remove.
 */
PRIVATE void del_slot(struct hole *prev_ptr, struct hole *hp) noexcept {
    if (hp == hole_head)
        hole_head = hp->h_next;
    else
        prev_ptr->h_next = hp->h_next;

    hp->h_next = free_slots;
    free_slots = hp;
}

/**
 * @brief Merge a hole with adjacent holes if they are contiguous.
 *
 * @param hp Pointer to the first hole in a chain to check.
 */
PRIVATE void merge(struct hole *hp) noexcept {

    register struct hole *next_ptr;

    /* If 'hp' points to the last hole, no merging is possible.  If it does not,
     * try to absorb its successor into it and free the successor's table entry.
     */
    if ((next_ptr = hp->h_next) == nullptr) // NIL_HOLE -> nullptr
        return;
    if (hp->h_base + hp->h_len == next_ptr->h_base) {
        hp->h_len += next_ptr->h_len; /* first one gets second one's mem */
        del_slot(hp, next_ptr);
    } else {
        hp = next_ptr;
    }

    /* If 'hp' now points to the last hole, return; otherwise, try to absorb its
     * succesor into it.
     */
    if ((next_ptr = hp->h_next) == nullptr) // NIL_HOLE -> nullptr
        return;
    if (hp->h_base + hp->h_len == next_ptr->h_base) {
        hp->h_len += next_ptr->h_len;
        del_slot(hp, next_ptr);
    }
}

/**
 * @brief Return the size of the largest available hole.
 */
[[nodiscard]] PUBLIC uint64_t max_hole() noexcept { // phys_clicks -> uint64_t
    /* Scan the hole list and return the largest hole. */

    register struct hole *hp;
    register uint64_t max; // phys_clicks -> uint64_t

    hp = hole_head;
    max = 0;
    while (hp != nullptr) { // NIL_HOLE -> nullptr
        if (hp->h_len > max)
            max = hp->h_len;
        hp = hp->h_next;
    }
    return (max);
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
PUBLIC void mem_init(uint64_t clicks) noexcept { // phys_clicks -> uint64_t
    for (auto it = std::begin(hole); it != std::end(hole); ++it) {
        auto next = std::next(it);
        it->h_next = next != std::end(hole) ? &(*next) : nullptr;
    }

    // Chain free slots using modern iteration. The first hole represents the
    // available memory region; remaining slots are placed on the free list.
    auto first = std::begin(hole);
    auto last = std::end(hole);
    for (auto it = std::next(first, 1); it != last; ++it) {
        auto next = std::next(it);
        it->h_next = (next != last) ? &(*next) : nullptr;
    }

    hole[0].h_next = nullptr; /* only 1 big hole initially */

    hole_head = &hole[0];
    free_slots = &hole[1];
    hole[0].h_base = 0;
    hole[0].h_len = clicks;
}
