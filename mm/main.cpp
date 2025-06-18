/* This file contains the main program of the memory manager and some related
 * procedures.  When MINIX starts up, the kernel runs for a little while,
 * initializing itself and its tasks, and then it runs MM.  MM at this point
 * does not know where FS is in memory and how big it is.  By convention, FS
 * must start at the click following MM, so MM can deduce where it starts at
 * least.  Later, when FS runs for the first time, FS makes a pseudo-call,
 * BRK2, to tell MM how big it is.  This allows MM to figure out where INIT
 * is.
 *
 * The entry points into this file are:
 *   main:	starts MM running
 *   reply:	reply to a process making an MM system call
 *   do_brk2:	pseudo-call for FS to report its size
 */

#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../include/vm.h"
#include "alloc.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "mproc.hpp"
#include "param.hpp"
#include <cstddef> // For std::size_t, uintptr_t
#include <cstdint> // For uint64_t, int64_t
#include <cstdio>  // For printf (used in do_brk2)
// #include <inttypes.h> // Not strictly needed if only %d is used for int

#define ENOUGH                                                                                     \
    static_cast<uint64_t>(4096) /* any # > max(FS size, INIT size) (phys_clicks -> uint64_t) */
#define CLICK_TO_K (1024L / CLICK_SIZE) /* convert clicks to K (CLICK_SIZE is int) */

static uint64_t tot_mem;    // PRIVATE phys_clicks -> static uint64_t
extern int (*call_vec[])(); // Assuming call_vec functions still return int

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
/**
 * @brief Entry point for the memory manager.
 *
 * Initializes internal tables and enters the main request loop.
 * The function does not return during normal execution.
 */
int main() noexcept { // Added noexcept (was already int main())
    /* Main routine of the memory manager. */

    int error;

    mm_init(); /* initialize memory manager tables */

    /* This is MM's main loop-  get work and do it, forever and forever. */
    while (TRUE) {
        /* Wait for message. */
        get_work(); /* wait for an MM system call */
        mp = &mproc[who];

        /* Set some flags. */
        error = OK;
        dont_reply = FALSE;
        err_code = -999;

        /* If the call number is valid, perform the call. */
        if (mm_call < 0 || mm_call >= NCALLS)
            error = ErrorCode::E_BAD_CALL;
        else
            error = (*call_vec[mm_call])();

        /* Send the results back to the user to indicate completion. */
        if (dont_reply)
            continue; /* no reply for EXIT and WAIT */
        if (mm_call == EXEC && error == OK)
            continue;
        reply(who, error, result2, res_ptr);
    }
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
/**
 * @brief Retrieve the next request from any process.
 *
 * Blocks until a message is received and sets global bookkeeping
 * variables describing the call.
 */
static void get_work() noexcept { // PRIVATE -> static, void return, noexcept
    /* Wait for the next message and extract useful information from it. */

    if (receive(ANY, &mm_in) != OK)
        panic("MM receive error", NO_NUM);
    who = mm_in.m_source; /* who sent the message */
    if (who < HARDWARE || who >= NR_PROCS)
        panic("MM called by", who);
    mm_call = mm_in.m_type; /* system call number */
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
/**
 * @brief Send a reply message to a process.
 *
 * @param proc_nr Target process number.
 * @param result Primary result code.
 * @param res2 Secondary result code.
 * @param respt Optional pointer result.
 */
// Modernized K&R, added void return, noexcept
PUBLIC void reply(int proc_nr, int result, int res2, char *respt) noexcept {
    /* Send a reply to a user process. */

    register struct mproc *proc_ptr;

    /* To make MM robust, check to see if destination is still alive. */
    proc_ptr = &mproc[proc_nr];
    if ((proc_ptr->mp_flags & IN_USE) == 0 || (proc_ptr->mp_flags & HANGING))
        return;
    reply_type = result;
    reply_i1 = res2;
    reply_p1 = respt;
    if (send(proc_nr, &mm_out) != OK)
        panic("MM can't reply", NO_NUM);
}

/*===========================================================================*
 *				mm_init					     *
 *===========================================================================*/
/**
 * @brief Initialize memory manager bookkeeping.
 *
 * Queries total memory from the kernel and sets up paging and virtual
 * memory before processes begin executing.
 */
static void mm_init() noexcept { // PRIVATE -> static, void return, noexcept
    /* Initialize the memory manager. */

    // Assuming modernized signatures for these extern functions from other modules
    extern uint64_t get_tot_mem() noexcept;
    extern void mm_paging_init() noexcept;
    extern void vm_init() noexcept;

    /* Find out how much memory the machine has and set up core map.  MM and FS
     * are part of the map.  Tell the kernel.
     */
    tot_mem = get_tot_mem(); /* # clicks in mem starting at absolute 0 (tot_mem is uint64_t) */
    mem_init(tot_mem);       /* initialize tables to all physical mem (mem_init takes uint64_t) */
    mm_paging_init();
    vm_init(); /* initialize virtual memory subsystem */

    /* Initialize MM's tables. */
    mproc[MM_PROC_NR].mp_flags |= IN_USE;
    mproc[FS_PROC_NR].mp_flags |= IN_USE;
    mproc[INIT_PROC_NR].mp_flags |= IN_USE;
    procs_in_use = 3;

    /* Set stack limit, which is checked on every procedure call. */
}

/*===========================================================================*
 *				do_brk2	   				     *
 *===========================================================================*/
/**
 * @brief Process the BRK2 pseudo call from the file system.
 *
 * Updates internal memory statistics based on INIT and RAM disk information.
 *
 * @return OK on success or an error code otherwise.
 */
PUBLIC int do_brk2() noexcept { // Added int return type (was implicit), noexcept
    /* This "call" is made once by FS during system initialization and then never
     * again by anyone.  It contains the origin and size of INIT, and the combined
     * size of the 1536 bytes of unused mem, MINIX and RAM disk.
     *   m1_i1 = size of INIT text in clicks (int in message)
     *   m1_i2 = size of INIT data in clicks (int in message)
     *   m1_i3 = total size of MINIX + RAM disk in clicks (int in message)
     *   m1_p1 = origin of INIT in clicks (char* in message, treated as address value)
     */

    int mem1, mem2, mem3; // For printf output in K
    register struct mproc *rmp;
    uint64_t init_org, init_clicks, ram_base, ram_clicks, tot_clicks; // phys_clicks -> uint64_t
    uint64_t init_text_clicks, init_data_clicks;                      // phys_clicks -> uint64_t

    if (who != FS_PROC_NR)
        return (ErrorCode::EPERM); /* only FS make do BRK2 */

    /* Remove the memory used by MINIX and RAM disk from the memory map. */
    // Message fields are int or char*. Internal types are uint64_t.
    init_text_clicks = static_cast<uint64_t>(mm_in.m1_i1());
    init_data_clicks = static_cast<uint64_t>(mm_in.m1_i2());
    tot_clicks = static_cast<uint64_t>(mm_in.m1_i3());
    init_org = reinterpret_cast<uint64_t>(reinterpret_cast<uintptr_t>(mm_in.m1_p1()));

    init_clicks = init_text_clicks + init_data_clicks;
    ram_base = init_org + init_clicks;  /* start of RAM disk */
    ram_clicks = tot_clicks - ram_base; /* size of RAM disk */
    alloc_mem(tot_clicks);              /* remove RAM disk from map (alloc_mem takes uint64_t) */

    /* Print memory information. */
    // tot_mem, ram_base, ram_clicks are uint64_t. CLICK_TO_K is long.
    // Results for mem1, mem2, mem3 should fit in int for %d.
    mem1 = static_cast<int>(tot_mem / CLICK_TO_K);
    mem2 = static_cast<int>((ram_base + 512 / CLICK_SIZE) / CLICK_TO_K); // CLICK_SIZE is int
    mem3 = static_cast<int>(ram_clicks / CLICK_TO_K);
    printf("%c 8%c~0", 033, 033); /* go to top of screen and clear screen */
    printf("Memory size = %dK     ", mem1);
    printf("MINIX = %dK     ", mem2);
    printf("RAM disk = %dK     ", mem3);
    printf("Available = %dK\n\n", mem1 - mem2 - mem3);
    if (mem1 - mem2 - mem3 < 32) {
        printf("\nNot enough memory to run MINIX\n\n", NO_NUM);
        sys_abort();
    }

    /* Initialize INIT's table entry. */
    rmp = &mproc[INIT_PROC_NR];
    // mem_phys is uint64_t. init_org, init_text_clicks, init_data_clicks are uint64_t.
    rmp->mp_seg[T].mem_phys = init_org;
    // mem_len is std::size_t (vir_clicks). init_text_clicks is uint64_t (phys_clicks).
    // This assignment assumes physical click count can represent virtual click count.
    rmp->mp_seg[T].mem_len = static_cast<std::size_t>(init_text_clicks);
    rmp->mp_seg[D].mem_phys = init_org + init_text_clicks;
    rmp->mp_seg[D].mem_len = static_cast<std::size_t>(init_data_clicks);
    // mem_vir is std::size_t (vir_clicks). init_clicks is uint64_t (phys_clicks).
    rmp->mp_seg[S].mem_vir = static_cast<std::size_t>(init_clicks);
    rmp->mp_seg[S].mem_phys = init_org + init_clicks;
    if (init_text_clicks != 0)
        rmp->mp_flags |= SEPARATE;

    return (OK);
}

/*===========================================================================*
 *				set_map	   				     *
 *===========================================================================*/
// Modernized K&R, added noexcept
/**
 * @brief Configure the memory map for a process.
 *
 * @param proc_nr Process table index.
 * @param base    Starting physical click.
 * @param clicks  Number of clicks to allocate.
 */
static void set_map(int proc_nr, uint64_t base, uint64_t clicks) noexcept {
    // proc_nr is int. base, clicks are phys_clicks (uint64_t).
    /* Set up the memory map as part of the system initialization. */

    register struct mproc *rmp;
    std::size_t vclicks; // vir_clicks -> std::size_t

    rmp = &mproc[proc_nr];
    vclicks = static_cast<std::size_t>(
        clicks); // Convert phys_clicks (uint64_t) to vir_clicks (std::size_t)

    // p_map members: mem_vir (std::size_t), mem_len (std::size_t), mem_phys (uint64_t)
    rmp->mp_seg[T].mem_vir = 0;
    rmp->mp_seg[T].mem_len = 0;
    rmp->mp_seg[T].mem_phys = base; // base is uint64_t
    rmp->mp_seg[D].mem_vir = 0;
    rmp->mp_seg[D].mem_len = vclicks;
    rmp->mp_seg[D].mem_phys = base;
    rmp->mp_seg[S].mem_vir = vclicks;
    rmp->mp_seg[S].mem_len = 0; // Stack length initially 0, grows down
    rmp->mp_seg[S].mem_phys =
        base + vclicks; // base is uint64_t, vclicks is std::size_t (promotes to uint64_t)
    sys_newmap(proc_nr, rmp->mp_seg);
}
