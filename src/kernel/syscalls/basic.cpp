/**
 * @file basic.cpp
 * @brief Basic syscall implementations
 *
 * Implements fundamental syscalls: write, getpid, exit
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "../syscall_table.hpp"
#include "../scheduler.hpp"
#include "../pcb.hpp"
#include "../early/serial_16550.hpp"
#include <cstdio>
#include <cstring>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// sys_write - Write to file descriptor
// ============================================================================

/**
 * @brief Write to file descriptor
 *
 * Week 8 Phase 4: Simple implementation that writes to serial console.
 * Future versions will delegate to VFS server via Lattice IPC.
 *
 * @param fd File descriptor (1=stdout, 2=stderr)
 * @param buf_addr User buffer address
 * @param count Number of bytes to write
 * @return Number of bytes written or negative error code
 */
int64_t sys_write(uint64_t fd, uint64_t buf_addr, uint64_t count,
                  uint64_t, uint64_t, uint64_t) {
    // Validate file descriptor
    if (fd != 1 && fd != 2) {
        return -EBADF;  // Bad file descriptor
    }

    // Validate count
    if (count == 0) {
        return 0;
    }

    if (count > 4096) {
        // Limit write size to prevent abuse
        count = 4096;
    }

    // Validate buffer address (basic check)
    // TODO: Proper user memory validation
    if (buf_addr == 0) {
        return -EFAULT;
    }

    const char* buf = reinterpret_cast<const char*>(buf_addr);

    // Week 8: Write directly to serial console
    // Future: Delegate to VFS server via Lattice IPC
    for (uint64_t i = 0; i < count; i++) {
        early_serial.write_char(buf[i]);
    }

    return (int64_t)count;  // Return bytes written
}

// ============================================================================
// sys_getpid - Get process ID
// ============================================================================

/**
 * @brief Get process ID of calling process
 *
 * @return pid_t (as int64_t)
 */
int64_t sys_getpid(uint64_t, uint64_t, uint64_t,
                   uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();

    if (!current) {
        // Should never happen
        return -1;
    }

    return (int64_t)current->pid;
}

// ============================================================================
// sys_exit - Terminate calling process
// ============================================================================

/**
 * @brief Terminate calling process
 *
 * Week 8 Phase 4: Simple implementation that marks process as ZOMBIE.
 * Future versions will:
 * - Free process resources
 * - Notify parent via wait/waitpid
 * - Handle orphaned children
 *
 * @param status Exit status code
 * @return Does not return
 */
[[noreturn]] int64_t sys_exit(uint64_t status, uint64_t, uint64_t,
                               uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();

    if (!current) {
        // Should never happen, but handle gracefully
        for(;;) { __asm__ volatile ("hlt"); }
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[SYSCALL] Process %d (%s) exiting with status %lu\n",
             current->pid, current->name, status);
    early_serial.write(buffer);

    // Mark process as ZOMBIE
    current->state = ProcessState::ZOMBIE;

    // TODO: Free resources, notify parent, handle children

    // Yield to next process
    schedule();

    // Should never return, but if we do, halt
    for(;;) { __asm__ volatile ("hlt"); }
}

} // namespace xinim::kernel
