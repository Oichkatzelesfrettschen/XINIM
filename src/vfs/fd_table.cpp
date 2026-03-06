/**
 * @file fd_table.cpp
 * @brief VFS-server global file descriptor table (ADR-0009)
 *
 * A single process-global FD table with MAX_FDS=64 entries.
 * Per-process FD tables are a v1.4.0 concern; for now the root
 * process has exclusive access to all file descriptors.
 *
 * Distinct from src/kernel/fd_table.cpp which is the per-process
 * per-hosted-runtime table; this one is freestanding and lives in
 * the VFS server address space.
 */

#include "bare_vfs.hpp"
#include <cstring>

// ============================================================================
// Global storage
// ============================================================================

static FdEntry g_fd_table[MAX_FDS];

// ============================================================================
// Public API
// ============================================================================

void fd_table_init() {
    __builtin_memset(g_fd_table, 0, sizeof(g_fd_table));
    // ino == 0 marks every slot as free
}

// Allocate a new FD for the given inode and flags.
// Returns fd index [0, MAX_FDS) on success, -1 (EMFILE) if table full.
int fd_allocate(uint32_t ino, uint32_t flags) {
    if (ino == 0) return -1; // cannot open the invalid inode
    for (int fd = 0; fd < static_cast<int>(MAX_FDS); ++fd) {
        if (g_fd_table[fd].ino == 0) {
            g_fd_table[fd].ino   = ino;
            g_fd_table[fd].flags = flags;
            g_fd_table[fd].pos   = 0;
            return fd;
        }
    }
    return -1; // EMFILE
}

// Return pointer to FD entry. Returns nullptr for invalid or closed FD.
FdEntry* fd_get(int fd) {
    if (fd < 0 || fd >= static_cast<int>(MAX_FDS)) return nullptr;
    if (g_fd_table[fd].ino == 0) return nullptr; // not open
    return &g_fd_table[fd];
}

// Force a specific fd slot to point at the given inode/flags/pos.
// If the slot is already occupied it is silently released first (caller
// must handle close-on-dup2 semantics before calling).
// Returns newfd on success, -1 on invalid newfd.
int fd_allocate_at(int newfd, uint32_t ino, uint32_t flags, int64_t pos) {
    if (newfd < 0 || newfd >= static_cast<int>(MAX_FDS)) return -1;
    if (ino == 0) return -1;
    g_fd_table[newfd].ino   = ino;
    g_fd_table[newfd].flags = flags;
    g_fd_table[newfd].pos   = pos;
    return newfd;
}

// Close and release FD. Returns 0 on success, -1 on bad fd.
int fd_release(int fd) {
    if (fd < 0 || fd >= static_cast<int>(MAX_FDS)) return -1;
    if (g_fd_table[fd].ino == 0) return -1; // already closed (EBADF)
    __builtin_memset(&g_fd_table[fd], 0, sizeof(FdEntry));
    return 0;
}
