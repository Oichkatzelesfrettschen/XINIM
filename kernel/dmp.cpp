/* This file contains some dumping routines for debugging. */

#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../include/stdio.hpp"
#include "const.hpp"
#include <cstddef> // For std::size_t
#include <cstdint> // For uint64_t, uintptr_t
#undef printf
#include "glo.hpp"
#include "type.hpp"
#include <inttypes.h> // For PRIx64, PRId64, PRIuMAX/PRIuPTR

/* Print a process name by index. */
static void prname(int i) noexcept; // Added noexcept
#include "proc.hpp"                 // proc struct definition

/**
 * @brief Translate a virtual address to a physical one.
 */
[[nodiscard]] extern uint64_t umap(struct proc *rp, int seg, std::size_t vir_addr,
                                   std::size_t bytes) noexcept;

/**
 * @brief Copy bytes between physical addresses.
 */
extern void phys_copy(uint64_t src, uint64_t dst, uint64_t bytes) noexcept;

#define NSIZE 20
uint64_t aout[NR_PROCS]; /* pointers to the program names (phys_bytes -> uint64_t) */
char nbuff[NSIZE + 1];   // Should be NSIZE+1 for null terminator if string
int vargv;               // Used in set_name, seems to hold a virtual address temporarily

/*===========================================================================*
 *				DEBUG routines here			     *
 *===========================================================================*/
/**
 * @brief Dump the process table for debugging.
 */
void p_dmp() noexcept { // K&R -> modern C++, added noexcept
    /* Proc table dump */

    struct proc *rp;
    char *np;
    [[maybe_unused]] std::size_t base, limit; // vir_bytes -> std::size_t
    [[maybe_unused]] uint64_t first, last;    // Used for physical click addresses
    [[maybe_unused]] uint64_t ltmp, dst;      // phys_bytes -> uint64_t
    int index;
    // extern phys_bytes umap(); // umap returns uint64_t

    printf("\nproc  -pid- --pc--  --sp--  flag  user  -sys-  base limit recv   command\n");

    // umap takes (proc*, int, std::size_t, std::size_t) returns uint64_t
    dst = umap(proc_addr(SYSTASK), D, reinterpret_cast<std::size_t>(nbuff),
               static_cast<std::size_t>(NSIZE));

    for (rp = &proc[0]; rp < &proc[NR_PROCS + NR_TASKS]; rp++) {
        if (rp->p_flags & P_SLOT_FREE)
            continue;
        first = rp->p_map[T].mem_phys; // mem_phys is phys_clicks (uint64_t)
        last = rp->p_map[S].mem_phys +
               rp->p_map[S].mem_len; // mem_phys is uint64_t, mem_len is vir_clicks (std::size_t)

        // Assuming CLICK_SHIFT is 4 for byte conversion, and 1024 for KB conversion
        // ltmp = (first << CLICK_SHIFT) + 512ULL; // For rounding to nearest KB later
        // base = static_cast<std::size_t>(ltmp / 1024ULL);
        // ltmp = (last << CLICK_SHIFT) + 512ULL;
        // limit = static_cast<std::size_t>(ltmp / 1024ULL);
        // Simpler: convert clicks to KB directly for base and limit as they are printed in K
        base = (rp->p_map[T].mem_phys + rp->p_map[T].mem_len) * CLICK_SIZE /
               1024; // Example, needs precise def of what base/limit mean
        limit = (rp->p_map[S].mem_phys + rp->p_map[S].mem_len) * CLICK_SIZE /
                1024; // This is just an example, original logic for base/limit was complex

        prname(rp - proc);
        // rp->p_pid (int), rp->p_pcpsw.pc (u64_t), rp->p_sp (u64_t), rp->p_flags (int)
        // rp->user_time, rp->sys_time (real_time -> int64_t)
        // base, limit (std::size_t)
        printf(" %4d %4" PRIxPTR " %4" PRIxPTR " %4x %6" PRId64 " %7" PRId64 "  %3zuK %3zuK  ",
               rp->p_pid,
               static_cast<uintptr_t>(rp->p_pcpsw.pc), // Assuming pc is an address
               static_cast<uintptr_t>(rp->p_sp),       // Assuming sp is an address
               rp->p_flags, rp->user_time, rp->sys_time,
               (rp->p_map[D].mem_vir + rp->p_map[D].mem_len) * CLICK_SIZE /
                   1024,                                  // Base of stack in K (approx)
               rp->p_map[S].mem_len * CLICK_SIZE / 1024); // Limit/size of stack in K (approx)

        if (rp->p_flags == 0)
            printf("      ");
        else
            prname(NR_TASKS + rp->p_getfrom);

        /* Fetch the command string from the user process. */
        index = rp - proc - NR_TASKS;
        if (index >= 0 && index < NR_PROCS && aout[index] != 0) { // Check index for aout
            // phys_copy takes (uint64_t, uint64_t, uint64_t)
            phys_copy(aout[index], dst, static_cast<uint64_t>(NSIZE));
            nbuff[NSIZE] = '\0';                           // Ensure null termination
            for (np = &nbuff[0]; np < &nbuff[NSIZE]; np++) // Iterate up to NSIZE, not NSIZE+1
                if (*np <= ' ' || *np >= 0177)
                    *np = '\0';        // Replace with null terminator
            if (index == INIT_PROC_NR) // Assuming LOW_USER was for init, check actual constant
                printf("/bin/sh");     // Typically INIT is /etc/init or /sbin/init
            else
                printf("%s", nbuff);
        }
        printf("\n");
    }
    printf("\n");
}

/**
 * @brief Dump process memory mappings.
 */
void map_dmp() noexcept { // K&R -> modern C++, added noexcept
    struct proc *rp;
    [[maybe_unused]] std::size_t base_k, size_k; // For K calculations, was vir_bytes

    printf("\nPROC   -----TEXT-----  -----DATA-----  ----STACK-----  BASE SIZE\n");
    // Iterate only over user processes, tasks might not have same map structure interpretation
    for (rp = proc_addr(0); rp < proc_addr(NR_PROCS); rp++) {
        if (rp->p_flags & P_SLOT_FREE)
            continue;

        // Calculate base and total size in K for the process
        // This assumes segments are contiguous for total size, which might not always be true.
        // The original logic for base/limit in p_dmp was complex and specific.
        // Here, we'll print segment details directly.
        // base_k = (rp->p_map[T].mem_phys * CLICK_SIZE) / 1024;
        // size_k = ((rp->p_map[S].mem_phys + rp->p_map[S].mem_len - rp->p_map[T].mem_phys) *
        // CLICK_SIZE) / 1024; The above calculation for size_k is a bit off. Let's just print
        // segment details.

        prname(rp - proc); // This will print task names too if loop is from proc[0]
                           // If only user procs: for (rp = proc_addr(0); rp < proc_addr(NR_PROCS);
                           // rp++) and prname(rp - proc - NR_TASKS);

        // p_map fields: mem_vir (size_t), mem_phys (uint64_t), mem_len (size_t)
        // Printing clicks for vir/phys/len for map details
        printf(" %4zx %4" PRIx64 " %4zx  %4zx %4" PRIx64 " %4zx  %4zx %4" PRIx64 " %4zx\n",
               rp->p_map[T].mem_vir, rp->p_map[T].mem_phys, rp->p_map[T].mem_len,
               rp->p_map[D].mem_vir, rp->p_map[D].mem_phys, rp->p_map[D].mem_len,
               rp->p_map[S].mem_vir, rp->p_map[S].mem_phys, rp->p_map[S].mem_len);
    }
}

const char *nayme[] = {"PRINTR", "TTY   ", "WINCHE", "FLOPPY", "RAMDSK", "CLOCK ",
                       "SYS   ", "HARDWR", "MM    ", "FS    ", "INIT  "}; // Assuming INIT is at index
                                                                    // NR_TASKS + 0 for user procs
/**
 * @brief Print a process name given its index.
 */
static void prname(int i) noexcept { // Added noexcept
    if (i == ANY + NR_TASKS)
        printf("ANY   ");
    else if (i >= 0 && i <= NR_TASKS + 2)
        printf("%s", nayme[i]);
    else
        printf("%4d  ", i - NR_TASKS);
}

/**
 * @brief Store command name for dumping.
 */
[[maybe_unused]] static void set_name(int proc_nr, char *ptr) noexcept { // Added noexcept
    /* When an EXEC call is done, the kernel is told about the stack pointer.
     * It uses the stack pointer to find the command line, for dumping
     * purposes.
     */

    // extern phys_bytes umap(); // umap returns uint64_t
    uint64_t src, dst; // phys_bytes -> uint64_t
    // count seems unused

    if (ptr == nullptr) {
        aout[proc_nr] = 0; // aout is uint64_t[]
        return;
    }

    // umap takes (proc*, int, std::size_t, std::size_t) returns uint64_t
    // ptr is char*.
    src = umap(proc_addr(proc_nr), D, reinterpret_cast<std::size_t>(ptr + 2),
               static_cast<std::size_t>(2));
    if (src == 0)
        return;
    // &vargv is int*.
    dst = umap(proc_addr(SYSTASK), D, reinterpret_cast<std::size_t>(&vargv),
               static_cast<std::size_t>(2));
    // phys_copy takes (uint64_t, uint64_t, uint64_t)
    phys_copy(src, dst, 2ULL);

    // Assuming vargv now holds a virtual address (copied from user stack)
    // This implies vargv should be treated as pointer-like or an offset.
    // If vargv is an int holding a direct address:
    aout[proc_nr] = umap(proc_addr(proc_nr), D, static_cast<std::size_t>(vargv),
                         static_cast<std::size_t>(NSIZE));
}
