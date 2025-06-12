/* This file is concerned with allocating and freeing arbitrary-size blocks of
 * physical memory on behalf of the FORK and EXEC system calls.  The key data
 * structure used is the hole table, which maintains a list of holes in memory.
 * It is kept sorted in order of increasing memory address. The addresses
 * it contains refer to physical memory, starting at absolute address 0
 * (i.e., they are not relative to the start of MM).  During system
 * initialization, that part of memory containing the interrupt vectors,
 * kernel, and MM are "allocated" to mark them as not available and to
 * remove them from the hole list.
 *
 * The entry points into this file are:
 *   alloc_mem:	allocate a given sized chunk of memory
 *   free_mem:	release a previously allocated chunk of memory
 *   mem_init:	initialize the tables when MM start up
 *   max_hole:	returns the largest hole currently available
 */

#include "../h/const.hpp"
#include "../h/type.hpp" // This should define phys_clicks as uint64_t
#include "const.hpp"     // For NO_MEM
#include <cstdint>       // For explicit uint64_t usage
#include <cstddef>       // For nullptr

constexpr int NR_HOLES = 128; /* max # entries in hole table */
// constexpr struct hole *NIL_HOLE = nullptr; // Definition removed

PRIVATE struct hole {
    uint64_t h_base;     /* where does the hole begin? (phys_clicks) */
    uint64_t h_len;      /* how big is the hole? (phys_clicks) */
    struct hole *h_next; /* pointer to next entry on the list */
} hole[NR_HOLES];

PRIVATE struct hole *hole_head;  /* pointer to first hole */
PRIVATE struct hole *free_slots; /* ptr to list of unused table slots */

/*===========================================================================*
 *				alloc_mem				     *
 *===========================================================================*/
[[nodiscard]] PUBLIC uint64_t alloc_mem(uint64_t clicks) noexcept { // phys_clicks -> uint64_t
    /* Allocate a block of memory from the free list using first fit. The block
     * consists of a sequence of contiguous bytes, whose length in clicks is
     * given by 'clicks'.  A pointer to the block is returned.  The block is
     * always on a click boundary.  This procedure is called when memory is
     * needed for FORK or EXEC.
     */

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

            /* The entire hole has been used up.  Manipulate free list. */
            del_slot(prev_ptr, hp);
            return (old_base);
        }

        prev_ptr = hp;
        hp = hp->h_next;
    }
    return (NO_MEM);
}

/*===========================================================================*
 *				free_mem				     *
 *===========================================================================*/
PUBLIC void free_mem(uint64_t base, uint64_t clicks) noexcept { // phys_clicks -> uint64_t
    /* Return a block of free memory to the hole list.  The parameters tell where
     * the block starts in physical memory and how big it is.  The block is added
     * to the hole list.  If it is contiguous with an existing hole on either end,
     * it is merged with the hole or holes.
     */

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

/*===========================================================================*
 *				del_slot				     *
 *===========================================================================*/
PRIVATE void del_slot(struct hole *prev_ptr, struct hole *hp) noexcept {
    /* Remove an entry from the hole list.  This procedure is called when a
     * request to allocate memory removes a hole in its entirety, thus reducing
     * the numbers of holes in memory, and requiring the elimination of one
     * entry in the hole list.
     */

    if (hp == hole_head)
        hole_head = hp->h_next;
    else
        prev_ptr->h_next = hp->h_next;

    hp->h_next = free_slots;
    free_slots = hp;
}

/*===========================================================================*
 *				merge					     *
 *===========================================================================*/
PRIVATE void merge(struct hole *hp) noexcept {
    /* Check for contiguous holes and merge any found.  Contiguous holes can occur
     * when a block of memory is freed, and it happens to abut another hole on
     * either or both ends.  The pointer 'hp' points to the first of a series of
     * three holes that can potentially all be merged together.
     */

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

/*===========================================================================*
 *				max_hole				     *
 *===========================================================================*/
// Return the size of the largest hole currently available.
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

/*===========================================================================*
 *				mem_init				     *
 *===========================================================================*/
PUBLIC void mem_init(uint64_t clicks) noexcept { // phys_clicks -> uint64_t
    /* Initialize hole lists.  There are two lists: 'hole_head' points to a linked
     * list of all the holes (unused memory) in the system; 'free_slots' points to
     * a linked list of table entries that are not in use.  Initially, the former
     * list has one entry, a single hole encompassing all of memory, and the second
     * list links together all the remaining table slots.  As memory becomes more
     * fragmented in the course of time (i.e., the initial big hole breaks up into
     * many small holes), new table slots are needed to represent them.  These
     * slots are taken from the list headed by 'free_slots'.
     */

    register struct hole *hp;

    for (hp = &hole[0]; hp < &hole[NR_HOLES]; hp++)
        hp->h_next = hp + 1;
    hole[0].h_next = nullptr; /* only 1 big hole initially */ // NIL_HOLE -> nullptr
    hole[NR_HOLES - 1].h_next = nullptr; // NIL_HOLE -> nullptr
    hole_head = &hole[0];
    free_slots = &hole[1];
    hole[0].h_base = 0;
    hole[0].h_len = clicks;
}
