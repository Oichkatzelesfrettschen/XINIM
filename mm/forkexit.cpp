/* This file deals with creating processes (via FORK) and deleting them (via
 * EXIT/WAIT). When a process forks, a new slot in the 'mproc' table is
 * allocated for it, and a copy of the parent's core image is made for the
 * child. Then the kernel and file system are informed. A process is removed
 * from the 'mproc' table when two events have occurred: (1) it has exited or
 * been killed by a signal, and (2) the parent has done a WAIT. If the process
 * exits first, it continues to occupy a slot until the parent does a WAIT.
 *
 * @section Description
 * This module orchestrates the lifecycle of processes within the XINIM operating
 * system, from creation via FORK to termination through EXIT/WAIT. It leverages
 * modern C++23 features, including RAII for process slot management, `std::span`
 * for safe table access, and `std::ranges` for expressive algorithms. The design
 * prioritizes type safety, resource management, and clarity, ensuring robust
 * process control.
 *
 * @section Entry Points
 * - do_fork: Perform the FORK system call, creating a new child process.
 * - do_mm_exit: Handle the EXIT system call, initiating process termination.
 * - mm_exit: Execute the core logic for process termination, including resource cleanup.
 * - do_wait: Implement the WAIT system call, allowing a parent to collect child status.
 *
 * @ingroup process_control
 */

#include "../h/callnr.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp" // Defines phys_clicks, vir_clicks, CLICK_SHIFT etc.
#include "alloc.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "mproc.hpp"
#include "param.hpp"
#include "process_slot.hpp" // For xinim::ScopedProcessSlot
#include "token.hpp"
#include <algorithm> // For std::ranges::any_of
#include <cstddef>   // For std::size_t
#include <cstdint>   // For uint64_t
#include <format>    // For std::format
#include <ranges>    // For std::ranges
#include <span>      // For std::span

// Last few slots reserved for superuser.
#define LAST_FEW 2

// Next PID to be assigned.
PRIVATE int next_pid = INIT_PROC_NR + 1;

// Forward declarations for internal helper functions.
PRIVATE void cleanup(struct mproc *child);
PUBLIC void mm_exit(struct mproc *rmp, int exit_status);

/*===========================================================================*
 * do_fork                                           *
 *===========================================================================*/
/**
 * @brief Perform the FORK system call.
 *
 * This function handles the creation of a new child process. It allocates a
 * new process table entry using `xinim::ScopedProcessSlot` for RAII-based
 * safety, duplicates the parent's memory image, and informs the kernel and
 * file system about the new child. The child's PID is then assigned and returned.
 *
 * @return Newly created child PID on success, or an error code from ::ErrorCode on failure.
 * @ingroup process_control
 */
PUBLIC int do_fork() {
    register struct mproc *rmp = mp;     ///< Pointer to the parent process entry.
    struct mproc *rmc = nullptr;         ///< Pointer to the child process entry.
    int child_nr;                        ///< Slot index of the child process.
    int t;                               ///< Temporary flag for PID assignment.
    std::span<mproc> proc_table{mproc, static_cast<std::size_t>(NR_PROCS)}; ///< Safe view of the process table.
    uint64_t prog_bytes;                 ///< Size of the program image in bytes.
    uint64_t prog_clicks;                ///< Size of the program image in clicks.
    uint64_t child_base;                 ///< Base physical address of the child's image.
    uint64_t parent_abs;                 ///< Physical address of the parent's image.
    uint64_t child_abs;                  ///< Physical address of the child's image.

    // Pre-check for table overflow to simplify recovery.
    if (procs_in_use == NR_PROCS || (procs_in_use >= NR_PROCS - LAST_FEW && rmp->mp_effuid != 0)) {
        return static_cast<int>(ErrorCode::EAGAIN);
    }

    // Determine memory to allocate for the child (T, D, S segments).
    prog_clicks = static_cast<uint64_t>(rmp->mp_seg[T].mem_len + rmp->mp_seg[D].mem_len + rmp->mp_seg[S].mem_len);
    prog_bytes = prog_clicks << CLICK_SHIFT;

    // Allocate physical memory for the child's image.
    if ((child_base = alloc_mem(prog_clicks)) == NO_MEM) {
        return static_cast<int>(ErrorCode::EAGAIN);
    }

    // Create a copy of the parent's core image for the child.
    child_abs = child_base << CLICK_SHIFT;
    parent_abs = rmp->mp_seg[T].mem_phys << CLICK_SHIFT;

    // Perform memory copy. Assuming mem_copy handles physical addresses as uintptr_t.
    int i = mem_copy(ABS, 0, static_cast<uintptr_t>(parent_abs), ABS, 0,
                     static_cast<uintptr_t>(child_abs), static_cast<std::size_t>(prog_bytes));
    if (i < 0) {
        panic(std::format("do_fork: can't copy memory (error code {})", i));
    }

    // Find and reserve a free process slot using RAII.
    // This ensures the slot is released if subsequent operations fail.
    xinim::ScopedProcessSlot child_slot{proc_table};
    if (!child_slot.valid()) {
        // Free the allocated memory if we can't get a process slot.
        free_mem(child_base, prog_clicks);
        return static_cast<int>(ErrorCode::EAGAIN);
    }

    rmc = child_slot.get();
    child_nr = child_slot.index();
    procs_in_use++; // Increment global counter now that slot is secured.

    // Set up the child's process table entry by copying from the parent.
    *rmc = *rmp;

    rmc->mp_parent = who;
    rmc->mp_seg[T].mem_phys = child_base;
    rmc->mp_seg[D].mem_phys = child_base + static_cast<uint64_t>(rmc->mp_seg[T].mem_len);
    rmc->mp_seg[S].mem_phys = rmc->mp_seg[D].mem_phys + (rmp->mp_seg[S].mem_phys - rmp->mp_seg[D].mem_phys);
    rmc->mp_exitstatus = 0;
    rmc->mp_sigstatus = 0;
    rmc->mp_token = generate_token(); // Generate a unique token for the child.

    // Find a free PID for the child.
    do {
        t = 0; // 't' = 0 means PID is still free.
        next_pid = (next_pid < 30000 ? next_pid + 1 : INIT_PROC_NR + 1);
        if (std::ranges::any_of(proc_table, [next_pid](const mproc &p) { return p.mp_pid == next_pid; })) {
            t = 1; // PID is already in use.
        }
        rmc->mp_pid = next_pid; // Assign PID to child.
    } while (t);

    // Commit the slot (release from RAII management, it's now a live process).
    child_slot.release();

    // Inform kernel and file system about the successful FORK.
    sys_fork(who, child_nr, rmc->mp_pid, rmc->mp_token);
    tell_fs(FORK, who, child_nr, 0);

    // Report child's memory map to kernel.
    sys_newmap(child_nr, rmc->mp_seg);

    // Reply to child to wake it up.
    reply(child_nr, 0, 0, NIL_PTR);
    return next_pid; // Return child's PID.
}

/*===========================================================================*
 * do_mm_exit                                        *
 *===========================================================================*/
/**
 * @brief Handle the EXIT system call.
 *
 * This function initiates the process termination sequence by calling
 * `mm_exit()`. The actual cleanup and resource release are handled by
 * `mm_exit()` and `cleanup()`.
 *
 * @return Always ::OK after scheduling the exit sequence.
 * @ingroup process_control
 */
PUBLIC int do_mm_exit() {
    mm_exit(mp, status);
    dont_reply = TRUE; // Don't reply to the newly terminated process.
    return OK;         // Pro forma return code.
}

/*===========================================================================*
 * mm_exit                                           *
 *===========================================================================*/
/**
 * @brief Actually terminate a process.
 *
 * This function handles the core logic for process termination. It stores the
 * exit status, and if the parent is waiting, it triggers resource cleanup.
 * Otherwise, the process is marked as `HANGING`. It also cancels any pending
 * alarms and informs the kernel and file system of the process's termination.
 *
 * @param rmp         Pointer to the process to be terminated.
 * @param exit_status The process's exit status (for its parent).
 * @ingroup process_control
 */
PUBLIC void mm_exit(struct mproc *rmp, int exit_status) {
    // Store the exit status in the process's mproc entry.
    rmp->mp_exitstatus = static_cast<char>(exit_status);

    // Determine termination action based on parent's waiting status.
    if (mproc[rmp->mp_parent].mp_flags & WAITING) {
        cleanup(rmp); // Parent is waiting, so clean up immediately.
    } else {
        rmp->mp_flags |= HANGING; // Parent not waiting, mark as hanging.
    }

    // If the exited process has a timer pending, kill it.
    if (rmp->mp_flags & ALARM_ON) {
        set_alarm(static_cast<int>(rmp - mproc.data()), 0);
    }

    // Tell the kernel and FS that the process is no longer runnable.
    sys_xit(rmp->mp_parent, static_cast<int>(rmp - mproc.data()));
    tell_fs(EXIT, static_cast<int>(rmp - mproc.data()), 0, 0); // File system can free the proc slot.
}

/*===========================================================================*
 * do_wait                                           *
 *===========================================================================*/
/**
 * @brief Implement the WAIT system call.
 *
 * This function allows a parent process to wait for one of its children to
 * terminate. It scans the process table for `HANGING` children. If a child
 * is found, its resources are cleaned up, and its status is returned. If no
 * children are `HANGING` but children exist, the parent is put into a `WAITING`
 * state.
 *
 * @return ::OK on success (either child collected or parent put to sleep),
 * or ::ErrorCode::ECHILD if the caller has no children.
 * @ingroup process_control
 */
PUBLIC int do_wait() {
    int children = 0; ///< Number of child processes.
    std::span<mproc> proc_table{mproc, static_cast<std::size_t>(NR_PROCS)}; ///< Safe view of the process table.

    // A process calling WAIT never gets a reply in the usual way via the
    // reply() in the main loop. If a child has already exited, the routine
    // cleanup() sends the reply to awaken the caller.

    // Check for a child that has already exited (HANGING).
    for (auto &proc : proc_table) {
        if ((proc.mp_flags & IN_USE) && proc.mp_parent == who) {
            ++children;
            if (proc.mp_flags & HANGING) {
                cleanup(&proc); // A child has already exited, clean it up.
                dont_reply = TRUE;
                return OK;
            }
        }
    }

    // No child has exited. Wait for one, unless none exists.
    if (children > 0) { // Does this process have any children?
        mp->mp_flags |= WAITING;
        dont_reply = TRUE;
        return OK; // Yes - wait for one to exit.
    }
    return static_cast<int>(ErrorCode::ECHILD); // No - parent has no children.
}

/*===========================================================================*
 * cleanup                                           *
 *===========================================================================*/
/**
 * @brief Release resources of a terminating process.
 *
 * This function is called to finalize the termination of a process. It frees
 * the memory occupied by the child, updates process flags, releases the process
 * table slot, and disinherits any children of the exiting process to `INIT`.
 * This routine is only invoked when a process has exited/been killed AND its
 * parent has performed a `WAIT`.
 *
 * @param child Pointer to the process that is exiting.
 * @ingroup process_control
 */
PRIVATE void cleanup(struct mproc *child) {
    std::span<mproc> proc_table{mproc, static_cast<std::size_t>(NR_PROCS)}; ///< Safe view of the process table.
    struct mproc *parent;
    int init_waiting;
    int child_nr;
    unsigned int r; // For status, not a target type for this change
    uint64_t s;     // phys_clicks -> uint64_t

    child_nr = static_cast<int>(child - proc_table.data());
    parent = &proc_table[child->mp_parent];

    // Wakeup the parent and send it the child's status.
    r = child->mp_sigstatus & 0377;
    r = r | (static_cast<unsigned int>(child->mp_exitstatus) << 8); // Ensure proper casting for bitwise ops.
    reply(child->mp_parent, child->mp_pid, r, NIL_PTR);

    // Release the memory occupied by the child.
    s = static_cast<uint64_t>(child->mp_seg[S].mem_vir + child->mp_seg[S].mem_len);
    if (child->mp_flags & SEPARATE) {
        s += static_cast<uint64_t>(child->mp_seg[T].mem_len);
    }
    free_mem(child->mp_seg[T].mem_phys, s); // Free the memory.

    // Update process flags and release the table slot.
    child->mp_flags &= ~HANGING;  // Turn off HANGING bit.
    child->mp_flags &= ~PAUSED;   // Turn off PAUSED bit.
    parent->mp_flags &= ~WAITING; // Parent is no longer waiting.
    child->mp_flags &= ~IN_USE;   // Release the table slot.
    procs_in_use--;

    // If the exiting process has children, disinherit them to INIT.
    init_waiting = (proc_table[INIT_PROC_NR].mp_flags & WAITING ? 1 : 0);
    for (auto &p : proc_table) {
        if (p.mp_parent == child_nr) {
            // 'p' points to a child to be disinherited.
            p.mp_parent = INIT_PROC_NR; // INIT takes over.
            if (init_waiting && (p.mp_flags & HANGING)) {
                // INIT was waiting, so clean up this disinherited child immediately.
                cleanup(&p); // Recursive call.
                init_waiting = 0; // Only one child can wake INIT.
            }
        }
    }
}