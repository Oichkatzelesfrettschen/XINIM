/**
 * @file syscall_table.cpp
 * @brief System call dispatch table implementation
 *
 * Routes syscalls from assembly handler to C++ implementations.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "syscall_table.hpp"
#include "early/serial_16550.hpp"
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// Forward Declarations of Syscall Handlers
// ============================================================================

// File I/O (from syscalls/file_ops.cpp and syscalls/basic.cpp)
int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                 uint64_t, uint64_t, uint64_t);
int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                  uint64_t, uint64_t, uint64_t);
int64_t sys_open(uint64_t pathname, uint64_t flags, uint64_t mode,
                 uint64_t, uint64_t, uint64_t);
int64_t sys_close(uint64_t fd, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence,
                  uint64_t, uint64_t, uint64_t);

// Advanced FD operations (from syscalls/fd_advanced.cpp - Week 9 Phase 3)
int64_t sys_pipe(uint64_t pipefd, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_dup(uint64_t oldfd, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg, uint64_t, uint64_t, uint64_t);

// Process management (from syscalls/basic.cpp and syscalls/process_mgmt.cpp)
int64_t sys_getpid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_getppid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_fork(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_wait4(uint64_t pid, uint64_t status, uint64_t options, uint64_t rusage,
                  uint64_t, uint64_t);
int64_t sys_exit(uint64_t status, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// Placeholder for unimplemented syscalls
static int64_t sys_unimplemented(uint64_t, uint64_t, uint64_t,
                                  uint64_t, uint64_t, uint64_t) {
    early_serial.write("[SYSCALL] Unimplemented syscall called\n");
    return -ENOSYS;
}

// ============================================================================
// Syscall Dispatch Table
// ============================================================================

/**
 * @brief Global syscall dispatch table
 *
 * Maps syscall numbers to handler functions.
 * Initialized at compile time using designated initializers.
 */
static SyscallHandler g_syscall_table[MAX_SYSCALLS] = {
    // File I/O (Week 9 Phase 1: VFS-integrated)
    [0]  = sys_read,           // read
    [1]  = sys_write,          // write (updated for VFS)
    [2]  = sys_open,           // open
    [3]  = sys_close,          // close
    [8]  = sys_lseek,          // lseek

    // Advanced FD operations (Week 9 Phase 3)
    [22] = sys_pipe,           // pipe
    [32] = sys_dup,            // dup
    [33] = sys_dup2,           // dup2
    [72] = sys_fcntl,          // fcntl

    // Process management (Week 9 Phase 2)
    [39] = sys_getpid,         // getpid
    [57] = sys_fork,           // fork
    [59] = sys_unimplemented,  // execve (Week 10)
    [60] = sys_exit,           // exit
    [61] = sys_wait4,          // wait4
    [110] = sys_getppid,       // getppid

    // All other entries are nullptr by default
};

// ============================================================================
// Syscall Statistics (for debugging)
// ============================================================================

static uint64_t g_syscall_count[MAX_SYSCALLS] = {0};
static uint64_t g_total_syscalls = 0;

/**
 * @brief Get total syscall count
 */
uint64_t get_total_syscall_count() {
    return g_total_syscalls;
}

/**
 * @brief Get count for specific syscall
 */
uint64_t get_syscall_count(uint64_t syscall_num) {
    if (syscall_num >= MAX_SYSCALLS) {
        return 0;
    }
    return g_syscall_count[syscall_num];
}

} // namespace xinim::kernel

// ============================================================================
// Syscall Dispatch Function (extern "C" for assembly)
// ============================================================================

/**
 * @brief Dispatch syscall to appropriate handler
 *
 * This function is called from the assembly syscall_handler.
 * It validates the syscall number and calls the corresponding handler.
 *
 * @param syscall_num Syscall number (from RAX)
 * @param arg1-arg6 Syscall arguments (from RDI, RSI, RDX, R10, R8, R9)
 * @return int64_t - Return value (>= 0) or error code (< 0)
 */
extern "C" int64_t syscall_dispatch(uint64_t syscall_num,
                                     uint64_t arg1, uint64_t arg2,
                                     uint64_t arg3, uint64_t arg4,
                                     uint64_t arg5, uint64_t arg6) {
    using namespace xinim::kernel;

    // Update statistics
    g_total_syscalls++;
    if (syscall_num < MAX_SYSCALLS) {
        g_syscall_count[syscall_num]++;
    }

    // Validate syscall number
    if (syscall_num >= MAX_SYSCALLS) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer),
                 "[SYSCALL] Invalid syscall number: %lu\n", syscall_num);
        early_serial.write(buffer);
        return -ENOSYS;
    }

    // Get handler
    SyscallHandler handler = g_syscall_table[syscall_num];
    if (!handler) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer),
                 "[SYSCALL] Unimplemented syscall: %lu\n", syscall_num);
        early_serial.write(buffer);
        return -ENOSYS;
    }

    // Log syscall (for debugging Week 8)
    #ifdef SYSCALL_DEBUG
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
             "[SYSCALL] %lu(0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx)\n",
             syscall_num, arg1, arg2, arg3, arg4, arg5, arg6);
    early_serial.write(buffer);
    #endif

    // Call handler
    int64_t result = handler(arg1, arg2, arg3, arg4, arg5, arg6);

    #ifdef SYSCALL_DEBUG
    snprintf(buffer, sizeof(buffer),
             "[SYSCALL] %lu returned %ld\n", syscall_num, result);
    early_serial.write(buffer);
    #endif

    return result;
}
