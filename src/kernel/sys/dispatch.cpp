#include <stdint.h>
#include <xinim/sys/syscalls.h>
#include "../early/serial_16550.hpp"
#include "../time/monotonic.hpp"

extern xinim::early::Serial16550 early_serial;

// ============================================================================
// Debug/Legacy Syscalls
// ============================================================================

static uint64_t sys_debug_write_impl(const char* s, uint64_t n) {
    for (uint64_t i=0;i<n;i++) early_serial.write_char(s[i]);
    return n;
}

static uint64_t sys_monotonic_ns_impl() {
    return xinim::time::monotonic_ns();
}

// ============================================================================
// File I/O Syscalls (delegated to VFS server via lattice IPC)
// ============================================================================

static int64_t sys_open_impl(const char* path, int flags, int mode) {
    // TODO: Send IPC message to VFS server
    // For now, return stub error
    return -1; // ENOENT
}

static int64_t sys_close_impl(int fd) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_read_impl(int fd, void* buf, uint64_t count) {
    // TODO: Send IPC message to VFS server
    // Special case: fd 0 (stdin) - for now, return 0 (EOF)
    if (fd == 0) return 0;
    return -1;
}

static int64_t sys_write_impl(int fd, const void* buf, uint64_t count) {
    // Special case: fd 1 (stdout) and fd 2 (stderr) - write to serial
    if (fd == 1 || fd == 2) {
        return sys_debug_write_impl((const char*)buf, count);
    }
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_lseek_impl(int fd, int64_t offset, int whence) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_stat_impl(const char* path, void* statbuf) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_fstat_impl(int fd, void* statbuf) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_access_impl(const char* path, int mode) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_dup_impl(int fd) {
    // TODO: Implement in process table
    return -1;
}

static int64_t sys_dup2_impl(int oldfd, int newfd) {
    // TODO: Implement in process table
    return -1;
}

static int64_t sys_pipe_impl(int pipefd[2]) {
    // TODO: Create pipe via VFS server
    return -1;
}

static int64_t sys_ioctl_impl(int fd, uint64_t request, void* argp) {
    // TODO: Send IPC message to device driver
    return -1;
}

static int64_t sys_fcntl_impl(int fd, int cmd, uint64_t arg) {
    // TODO: Implement in process table
    return -1;
}

// ============================================================================
// Directory Operations
// ============================================================================

static int64_t sys_mkdir_impl(const char* path, int mode) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_rmdir_impl(const char* path) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_chdir_impl(const char* path) {
    // TODO: Update current process working directory
    return -1;
}

static int64_t sys_getcwd_impl(char* buf, uint64_t size) {
    // TODO: Read from current process working directory
    return -1;
}

static int64_t sys_link_impl(const char* oldpath, const char* newpath) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_unlink_impl(const char* path) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_rename_impl(const char* oldpath, const char* newpath) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_chmod_impl(const char* path, int mode) {
    // TODO: Send IPC message to VFS server
    return -1;
}

static int64_t sys_chown_impl(const char* path, int uid, int gid) {
    // TODO: Send IPC message to VFS server
    return -1;
}

// ============================================================================
// Process Management
// ============================================================================

static void sys_exit_impl(int status) {
    // TODO: Terminate current process via process manager
    // This should not return
    while (1) {} // Halt for now
}

static int64_t sys_fork_impl() {
    // TODO: Create new process via process manager
    return -1;
}

static int64_t sys_execve_impl(const char* filename, char* const argv[], char* const envp[]) {
    // TODO: Load new program via VFS and process manager
    return -1;
}

static int64_t sys_wait4_impl(int pid, int* status, int options, void* rusage) {
    // TODO: Wait for child process
    return -1;
}

static int64_t sys_getpid_impl() {
    // TODO: Return current process ID from process table
    return 1; // Stub: return PID 1
}

static int64_t sys_getppid_impl() {
    // TODO: Return parent process ID from process table
    return 0; // Stub: return PID 0 (kernel)
}

static int64_t sys_kill_impl(int pid, int sig) {
    // TODO: Send signal to process via process manager
    return -1;
}

static int64_t sys_signal_impl(int signum, uint64_t handler) {
    // TODO: Register signal handler in process table
    return -1;
}

static int64_t sys_sigaction_impl(int signum, const void* act, void* oldact) {
    // TODO: Register signal handler in process table
    return -1;
}

// ============================================================================
// Memory Management
// ============================================================================

static int64_t sys_brk_impl(void* addr) {
    // TODO: Adjust process heap via memory manager
    return -1;
}

static int64_t sys_mmap_impl(void* addr, uint64_t length, int prot, int flags, int fd, int64_t offset) {
    // TODO: Map memory via memory manager
    return -1;
}

static int64_t sys_munmap_impl(void* addr, uint64_t length) {
    // TODO: Unmap memory via memory manager
    return -1;
}

static int64_t sys_mprotect_impl(void* addr, uint64_t length, int prot) {
    // TODO: Change memory protection via memory manager
    return -1;
}

// ============================================================================
// IPC (XINIM Lattice IPC)
// ============================================================================

static int64_t sys_lattice_connect_impl(const char* name, int flags, int mode) {
    // TODO: Connect to lattice IPC endpoint
    return -1;
}

static int64_t sys_lattice_send_impl(int endpoint_id, const void* msg, uint64_t len, int prio) {
    // TODO: Send message via lattice IPC
    return -1;
}

static int64_t sys_lattice_recv_impl(int endpoint_id, void* msg, uint64_t len, int* prio) {
    // TODO: Receive message via lattice IPC
    return -1;
}

static int64_t sys_lattice_close_impl(int endpoint_id) {
    // TODO: Close lattice IPC endpoint
    return -1;
}

// ============================================================================
// Time
// ============================================================================

static int64_t sys_time_impl(int64_t* tloc) {
    // TODO: Get POSIX time from RTC
    int64_t t = xinim::time::monotonic_ns() / 1000000000; // Convert ns to seconds (stub)
    if (tloc) *tloc = t;
    return t;
}

static int64_t sys_gettimeofday_impl(void* tv, void* tz) {
    // TODO: Get time of day from RTC
    return -1;
}

static int64_t sys_clock_gettime_impl(int clk_id, void* tp) {
    // TODO: Get clock time (CLOCK_REALTIME, CLOCK_MONOTONIC)
    return -1;
}

static int64_t sys_nanosleep_impl(const void* req, void* rem) {
    // TODO: Sleep for specified time via scheduler
    return -1;
}

// ============================================================================
// User/Group IDs
// ============================================================================

static int64_t sys_getuid_impl() {
    // TODO: Get real user ID from process table
    return 0; // Stub: root
}

static int64_t sys_geteuid_impl() {
    // TODO: Get effective user ID from process table
    return 0; // Stub: root
}

static int64_t sys_getgid_impl() {
    // TODO: Get real group ID from process table
    return 0; // Stub: root
}

static int64_t sys_getegid_impl() {
    // TODO: Get effective group ID from process table
    return 0; // Stub: root
}

static int64_t sys_setuid_impl(int uid) {
    // TODO: Set user ID in process table
    return -1;
}

static int64_t sys_setgid_impl(int gid) {
    // TODO: Set group ID in process table
    return -1;
}

// ============================================================================
// Syscall Dispatcher
// ============================================================================

extern "C" uint64_t xinim_syscall_dispatch(uint64_t no,
    uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    switch (no) {
        // Debug/Legacy
        case SYS_debug_write: return sys_debug_write_impl((const char*)a0, a1);
        case SYS_monotonic_ns: return sys_monotonic_ns_impl();

        // File I/O
        case SYS_open: return sys_open_impl((const char*)a0, a1, a2);
        case SYS_close: return sys_close_impl(a0);
        case SYS_read: return sys_read_impl(a0, (void*)a1, a2);
        case SYS_write: return sys_write_impl(a0, (const void*)a1, a2);
        case SYS_lseek: return sys_lseek_impl(a0, a1, a2);
        case SYS_stat: return sys_stat_impl((const char*)a0, (void*)a1);
        case SYS_fstat: return sys_fstat_impl(a0, (void*)a1);
        case SYS_access: return sys_access_impl((const char*)a0, a1);
        case SYS_dup: return sys_dup_impl(a0);
        case SYS_dup2: return sys_dup2_impl(a0, a1);
        case SYS_pipe: return sys_pipe_impl((int*)a0);
        case SYS_ioctl: return sys_ioctl_impl(a0, a1, (void*)a2);
        case SYS_fcntl: return sys_fcntl_impl(a0, a1, a2);

        // Directory operations
        case SYS_mkdir: return sys_mkdir_impl((const char*)a0, a1);
        case SYS_rmdir: return sys_rmdir_impl((const char*)a0);
        case SYS_chdir: return sys_chdir_impl((const char*)a0);
        case SYS_getcwd: return sys_getcwd_impl((char*)a0, a1);
        case SYS_link: return sys_link_impl((const char*)a0, (const char*)a1);
        case SYS_unlink: return sys_unlink_impl((const char*)a0);
        case SYS_rename: return sys_rename_impl((const char*)a0, (const char*)a1);
        case SYS_chmod: return sys_chmod_impl((const char*)a0, a1);
        case SYS_chown: return sys_chown_impl((const char*)a0, a1, a2);

        // Process management
        case SYS_exit: sys_exit_impl(a0); return 0; // Never returns
        case SYS_fork: return sys_fork_impl();
        case SYS_execve: return sys_execve_impl((const char*)a0, (char* const*)a1, (char* const*)a2);
        case SYS_wait4: return sys_wait4_impl(a0, (int*)a1, a2, (void*)a3);
        case SYS_getpid: return sys_getpid_impl();
        case SYS_getppid: return sys_getppid_impl();
        case SYS_kill: return sys_kill_impl(a0, a1);
        case SYS_signal: return sys_signal_impl(a0, a1);
        case SYS_sigaction: return sys_sigaction_impl(a0, (const void*)a1, (void*)a2);

        // Memory management
        case SYS_brk: return sys_brk_impl((void*)a0);
        case SYS_mmap: return sys_mmap_impl((void*)a0, a1, a2, a3, a4, (int64_t)a4);
        case SYS_munmap: return sys_munmap_impl((void*)a0, a1);
        case SYS_mprotect: return sys_mprotect_impl((void*)a0, a1, a2);

        // IPC (XINIM lattice)
        case SYS_lattice_connect: return sys_lattice_connect_impl((const char*)a0, a1, a2);
        case SYS_lattice_send: return sys_lattice_send_impl(a0, (const void*)a1, a2, a3);
        case SYS_lattice_recv: return sys_lattice_recv_impl(a0, (void*)a1, a2, (int*)a3);
        case SYS_lattice_close: return sys_lattice_close_impl(a0);

        // Time
        case SYS_time: return sys_time_impl((int64_t*)a0);
        case SYS_gettimeofday: return sys_gettimeofday_impl((void*)a0, (void*)a1);
        case SYS_clock_gettime: return sys_clock_gettime_impl(a0, (void*)a1);
        case SYS_nanosleep: return sys_nanosleep_impl((const void*)a0, (void*)a1);

        // User/Group IDs
        case SYS_getuid: return sys_getuid_impl();
        case SYS_geteuid: return sys_geteuid_impl();
        case SYS_getgid: return sys_getgid_impl();
        case SYS_getegid: return sys_getegid_impl();
        case SYS_setuid: return sys_setuid_impl(a0);
        case SYS_setgid: return sys_setgid_impl(a0);

        default: return (uint64_t)-1; // ENOSYS
    }
}
