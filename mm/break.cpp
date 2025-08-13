/* The MINIX model of memory allocation reserves a fixed amount of memory for
 * the combined text, data, and stack segements.  The amount used for a child
 * process created by FORK is the same as the parent had.  If the child does
 * an EXEC later, the new size is taken from the header of the file EXEC'ed.
 *
 * The layout in memory consists of the text segment, followed by the data
 * segment, followed by a gap (unused memory), followed by the stack segment.
 * The data segment grows upward and the stack grows downward, so each can
 * take memory from the gap.  If they meet, the process must be killed.  The
 * procedures in this file deal with the growth of the data and stack segments.
 *
 * The entry points into this file are:
 *   do_brk:	  BRK/SBRK system calls to grow or shrink the data segment
 *   adjust:	  see if a proposed segment adjustment is allowed
 *   size_ok:	  see if the segment sizes are feasible
 *   stack_fault: grow the stack segment
 */

#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/signal.hpp"
#include "../h/type.hpp" // Defines vir_bytes, vir_clicks, CLICK_SIZE, CLICK_SHIFT
#include "const.hpp"
#include "glo.hpp"
#include "mproc.hpp"
#include "param.hpp"
#include <cstddef> // For std::size_t
#include <cstdint> // For int64_t

constexpr int DATA_CHANGED = 1;  /* flag value when data segment size changed */
constexpr int STACK_CHANGED = 2; /* flag value when stack size changed */

/*===========================================================================*
 *				do_brk  				     *
 *===========================================================================*/
/**
 * @brief Handle the BRK/SBRK system call.
 *
 * Resizes the data segment according to the address supplied via the incoming message. The stack
 * pointer is validated so that the stack and data segments do not collide.
 *
 * @return OK on success or an ErrorCode value otherwise.
 */
PUBLIC int do_brk() {
    /* Perform the brk(addr) system call.
     *
     * The call is complicated by the fact that on some machines (e.g., 8088),
     * the stack pointer can grow beyond the base of the stack segment without
     * anybody noticing it.    For a file not using separate I & D space,
     * the parameter, 'addr' is to the total size, text + data.  For a file using
     * separate text and data spaces, it is just the data size. Files using
     * separate I & D space have the SEPARATE bit in mp_flags set.
     */

    struct mproc *rmp;
    int r;
    std::size_t v, new_sp;  // vir_bytes -> std::size_t
    std::size_t new_clicks; // vir_clicks -> std::size_t

    rmp = mp;
    // Assuming addr (char*) can be meaningfully converted to a size/offset.
    // The original cast to vir_bytes (unsigned) then to long could lose info if pointers are >
    // long. Prefer casting directly to size_t if it's an offset, or uintptr_t if it's an address
    // value. For this specific formula, using v directly as size_t should be fine if it represents
    // a segment size.
    v = reinterpret_cast<std::size_t>(addr);          /* 'addr' is the new data segment size */
    new_clicks = (v + CLICK_SIZE - 1) >> CLICK_SHIFT; // Simplified calculation
    sys_getsp(who, &new_sp); /* ask kernel for current sp value, new_sp is std::size_t* */
    r = adjust(rmp, new_clicks, new_sp);
    res_ptr = (r == OK ? addr : (char *)-1);
    return (r); /* return new size or -1 */
}

/*===========================================================================*
 *				adjust  				     *
 *===========================================================================*/
/**
 * @brief Adjust the data and stack segments of a process.
 *
 * @param rmp        Process descriptor for the process being adjusted.
 * @param data_clicks Desired new data segment size in clicks.
 * @param sp         Current stack pointer value.
 *
 * @return OK if the new layout fits or ErrorCode::ENOMEM otherwise.
 */
[[nodiscard]] PUBLIC int adjust(struct mproc *rmp, std::size_t data_clicks,
                                std::size_t sp) { // vir_clicks, vir_bytes -> std::size_t
    /* See if data and stack segments can coexist, adjusting them if need be.
     * Memory is never allocated or freed.  Instead it is added or removed from the
     * gap between data segment and stack segment.  If the gap size becomes
     * negative, the adjustment of data or stack fails and ErrorCode::ENOMEM is returned.
     */

    struct mem_map *mem_sp, *mem_dp;
    std::size_t sp_click, gap_base, lower, old_clicks; // vir_clicks -> std::size_t
    int changed, r, ft;
    int64_t base_of_stack, delta; // Changed from long for clarity and defined width

    mem_dp = &rmp->mp_seg[D]; /* pointer to data segment map */
    mem_sp = &rmp->mp_seg[S]; /* pointer to stack segment map */
    changed = 0;              /* set when either segment changed */

    /* See if stack size has gone negative (i.e., sp too close to 0xFFFF...) */
    // mem_vir and mem_len are std::size_t (from vir_clicks in mem_map)
    base_of_stack = static_cast<int64_t>(mem_sp->mem_vir) + static_cast<int64_t>(mem_sp->mem_len);
    sp_click = sp >> CLICK_SHIFT; /* click containing sp (sp is std::size_t) */
    if (sp_click >= static_cast<std::size_t>(base_of_stack)) // Compare compatible types
        return (ErrorCode::ENOMEM);                          /* sp too high */

    /* Compute size of gap between stack and data segments. */
    delta = static_cast<int64_t>(mem_sp->mem_vir) - static_cast<int64_t>(sp_click);
    // lower and gap_base are std::size_t. Ensure delta comparison is safe or types are consistent.
    lower = (delta > 0 ? sp_click : mem_sp->mem_vir);
    gap_base = mem_dp->mem_vir + data_clicks;
    if (lower < gap_base)
        return (ErrorCode::ENOMEM); /* data and stack collided */

    /* Update data length (but not data orgin) on behalf of brk() system call. */
    old_clicks = mem_dp->mem_len;
    if (data_clicks != mem_dp->mem_len) {
        mem_dp->mem_len = data_clicks;
        changed |= DATA_CHANGED;
    }

    /* Update stack length and origin due to change in stack pointer. */
    if (delta > 0) {
        mem_sp->mem_vir -= delta;
        mem_sp->mem_phys -= delta;
        mem_sp->mem_len += delta;
        changed |= STACK_CHANGED;
    }

    /* Do the new data and stack segment sizes fit in the address space? */
    ft = (rmp->mp_flags & SEPARATE);
    r = size_ok(ft, rmp->mp_seg[T].mem_len, rmp->mp_seg[D].mem_len, rmp->mp_seg[S].mem_len,
                rmp->mp_seg[D].mem_vir, rmp->mp_seg[S].mem_vir);
    if (r == OK) {
        if (changed)
            sys_newmap(rmp - mproc, rmp->mp_seg);
        return (OK);
    }

    /* New sizes don't fit or require too many page/segment registers. Restore.*/
    if (changed & DATA_CHANGED)
        mem_dp->mem_len = old_clicks;
    if (changed & STACK_CHANGED) {
        mem_sp->mem_vir += delta;
        mem_sp->mem_phys += delta;
        mem_sp->mem_len -= delta;
    }
    return (ErrorCode::ENOMEM);
}

/*===========================================================================*
 *				size_ok  				     *
 *===========================================================================*/
/**
 * @brief Validate whether proposed segment sizes fit in memory.
 *
 * @param file_type  Indicates whether text and data are separate.
 * @param tc         Text size in clicks.
 * @param dc         Data size in clicks.
 * @param sc         Stack size in clicks.
 * @param dvir       Data segment virtual origin.
 * @param s_vir      Stack segment virtual origin.
 *
 * @return OK if the configuration fits, otherwise ErrorCode::ENOMEM.
 */
[[nodiscard]] PUBLIC int size_ok(int file_type, std::size_t tc, std::size_t dc, std::size_t sc,
                                 std::size_t dvir, std::size_t s_vir) { // vir_clicks -> std::size_t
    /* Check to see if the sizes are feasible and enough segmentation registers
     * exist.  On a machine with eight 8K pages, text, data, stack sizes of
     * (32K, 16K, 16K) will fit, but (33K, 17K, 13K) will not, even though the
     * former is bigger (64K) than the latter (63K).  Even on the 8088 this test
     * is needed, since the data and stack may not exceed 4096 clicks.
     */

    std::size_t pt, pd, ps; /* segment sizes in pages, should be size_t */

    // PAGE_SIZE is from mm/const.hpp (was int, ideally std::size_t)
    // Assuming PAGE_SIZE is compatible or made std::size_t.
    // tc, dc, sc are std::size_t. CLICK_SHIFT is int.
    pt = ((tc << CLICK_SHIFT) + static_cast<std::size_t>(PAGE_SIZE) - 1) /
         static_cast<std::size_t>(PAGE_SIZE);
    pd = ((dc << CLICK_SHIFT) + static_cast<std::size_t>(PAGE_SIZE) - 1) /
         static_cast<std::size_t>(PAGE_SIZE);
    ps = ((sc << CLICK_SHIFT) + static_cast<std::size_t>(PAGE_SIZE) - 1) /
         static_cast<std::size_t>(PAGE_SIZE);

    if (file_type == SEPARATE) {
        if (pt > MAX_PAGES || pd + ps > MAX_PAGES)
            return (ErrorCode::ENOMEM);
    } else {
        if (pt + pd + ps > MAX_PAGES)
            return (ErrorCode::ENOMEM);
    }

    if (dvir + dc > s_vir)
        return (ErrorCode::ENOMEM);

    return (OK);
}

/*===========================================================================*
 *				stack_fault  				     *
 *===========================================================================*/
/**
 * @brief Grow the stack segment to satisfy a fault.
 *
 * Invoked when a process faults on its stack. The stack is expanded until the
 * supplied stack pointer lies within the segment. If growth is impossible, the
 * process is terminated with SIGSEGV.
 *
 * @param proc_nr Index of the faulting process.
 */
PRIVATE void stack_fault(int proc_nr) {
    /* Handle a stack fault by growing the stack segment until sp is inside of it.
     * If this is impossible because data segment is in the way, kill the process.
     */

    struct mproc *rmp;
    int r;
    std::size_t new_sp; // vir_bytes -> std::size_t

    rmp = &mproc[proc_nr];
    sys_getsp(rmp - mproc, &new_sp); // new_sp is std::size_t*
    r = adjust(rmp, rmp->mp_seg[D].mem_len, new_sp);
    if (r == OK)
        return;

    /* Stack has bumped into data segment.  Kill the process. */
    rmp->mp_catch = 0;      /* don't catch this signal */
    sig_proc(rmp, SIGSEGV); /* terminate process */
}
