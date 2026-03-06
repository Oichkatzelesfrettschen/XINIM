# ADR-0009: Bare-Metal VFS Architecture

**Date:** 2026-03-05
**Status:** Accepted
**Deciders:** Xinim project maintainers
**Phase:** Research (v1.2.0) -- Implementation deferred to v1.3.0

---

## Context and Problem Statement

The existing VFS layer (`src/vfs/`) is incompatible with the freestanding kernel
because it uses hosted STL throughout:
- `ramfs.hpp`: `std::string`, `std::unordered_map`, `std::shared_ptr`, `std::vector`
- `vfs.cpp`: `std::mutex`, `std::string`, `std::unordered_map`, `std::map`
- `filesystem.cpp`, `mount.cpp`: `std::shared_ptr`, `std::mutex`, `std::string`

None of these are available in `-ffreestanding` compilation. The result is that
the VFS server returns ENOSYS for all file operations, giving Xinim 0% POSIX
compliance despite having the scaffolding.

The goal is to design a minimal, fast, RAM-efficient replacement that:
1. Compiles freestanding (no STL, no heap unless explicitly allocated via heap.hpp)
2. Supports the minimum POSIX surface needed for VFS server: open, read, write, close, stat, lseek
3. Uses fixed-size structures to avoid fragmentation
4. Is cache-line-friendly for fast path operations

---

## Survey: What Can Be Salvaged

| Component | Salvageable? | Reason |
|-----------|-------------|--------|
| `ramfs.hpp` RamfsNode metadata struct | YES (fields) | Field types are plain; class hierarchy and names are not |
| `ramfs.hpp` permission logic | YES | Can be ported to free functions |
| `vfs.cpp` path traversal algorithm | YES (logic) | Algorithm is correct; needs std::string removed |
| `mount.cpp` longest-prefix matching | YES (concept) | Replace std::map with sorted array |
| `filesystem.cpp` buffer cache | YES (concept) | Replace shared_ptr with explicit pool |
| `ext2.cpp`, `tmpfs.cpp` | NO | Too many STL dependencies; out of scope |
| `src/fs/` (legacy MINIX fs) | PARTIAL | inode.hpp field definitions useful as reference |

---

## Proposed Design

### 1. Inode Structure (64 bytes, one cache line)

```cpp
struct alignas(64) RawInode {
    uint32_t ino;           // Inode number (0 = free)
    uint16_t mode;          // File type + permissions (POSIX st_mode layout)
    uint16_t flags;         // Internal: IS_INLINE, IS_DIR, etc.
    uint32_t uid;           // Owner UID
    uint32_t gid;           // Owner GID
    uint32_t nlink;         // Hard link count
    uint64_t size;          // File size in bytes
    int64_t  mtime;         // Modification time (kernel ticks)
    int64_t  ctime;         // Status change time
    // Data: inline if size <= 24 bytes, else pointer to arena block
    union {
        uint8_t  inline_data[24]; // Small files stored directly (no extra alloc)
        uint64_t data_block_off;  // Offset into data_arena[] for larger files
    };
    // Total: 4+2+2+4+4+4+8+8+8+24 = 68 bytes; pad to 64 with field reordering
};
```

**Memory budget:** 1024 inodes * 64 bytes = 64 KB inode table.
Kept as a flat static array `RawInode g_inodes[MAX_INODES]`.

**WHY 64 bytes:** One L1 cache line. Reading an inode does not pull in
adjacent inodes. stat() and open() complete with one cache miss.

**WHY inline data:** Files <= 24 bytes (config stubs, pid files, /proc entries)
have zero additional allocation. Reduces malloc traffic for small files.

### 2. Directory Entry Format (fixed 32 bytes)

```cpp
struct DirEntry {
    uint32_t ino;       // Inode number of child (0 = unused slot)
    uint8_t  type;      // File type byte (DT_REG, DT_DIR, etc.)
    uint8_t  namelen;   // Length of name (0-30)
    char     name[26];  // Name bytes (NOT null-terminated beyond namelen)
};
```

**WHY fixed-size:** No heap allocation for directory traversal.
Directories are stored as a contiguous array of DirEntry in a pre-allocated
arena. Binary search is possible after sorting by name.

**WHY 26-char inline name:** Covers 95%+ of actual kernel-space paths.
Files with longer names (up to 255 chars) need an overflow name table
(separate arena, accessed via DirEntry with namelen=0xFF and ino pointing
to overflow entry). This is a v1.3.0+ concern; for ramfs 26 bytes is enough.

**Directory storage:** Each directory inode stores `data_block_off` pointing
into the dirent_arena. The block contains `(size / sizeof(DirEntry))` entries.

### 3. Mount Table (fixed array)

```cpp
constexpr int MAX_MOUNTS = 8;

struct MountEntry {
    uint32_t root_ino;      // Inode of mounted root (0 = free slot)
    char     path[56];      // Mount point path (absolute)
};

static MountEntry g_mounts[MAX_MOUNTS];
```

**Mount resolution:** Sorted by path length descending. Longest-prefix match
done with `strncmp` on each active entry. O(MAX_MOUNTS) = O(8) operations.
**WHY NOT std::map:** No heap, no STL; 8 mount points is sufficient for a
microkernel ramfs. If more are needed, bump MAX_MOUNTS.

### 4. Buffer Cache (pre-allocated pool)

```cpp
constexpr int CACHE_BLOCKS   = 64;
constexpr int CACHE_BLK_SIZE = 512;

struct CacheBlock {
    uint64_t block_num;         // Block number on device (UINT64_MAX = free)
    uint32_t device_id;
    uint32_t dirty : 1;
    uint32_t lru_seq : 31;      // LRU counter (incremented on each access)
    uint8_t  data[CACHE_BLK_SIZE];
};

static CacheBlock g_cache[CACHE_BLOCKS];
```

**LRU eviction:** On allocation, find the block with the lowest `lru_seq`.
O(CACHE_BLOCKS) scan = O(64). No heap allocation. No intrusive list (the
lru_seq counter approach avoids pointer manipulation in the eviction path).

**WHY pre-allocated:** Zero malloc during I/O is essential. The buffer cache
is one of the hottest paths in a filesystem. Pre-allocation also means the
worst-case memory usage is fixed and known at compile time.

### 5. Path Resolution (no heap)

Path resolution walks components in-place using two pointers into the path
buffer rather than copying into `std::string`. The walk tuple is:

```cpp
struct PathComponent {
    const char* start;  // Start of component in original path buffer
    uint8_t     len;    // Length (max 255)
};
```

No allocation. The path buffer is owned by the caller (typically the
kernel syscall handler which holds it on its stack). Resolution produces
a final `ino` -- callers store the inode number, not a pointer.

**WHY inode numbers, not pointers:** Pointers into a fixed array are safe,
but inode numbers are more resilient to pool reallocation and are the POSIX-
canonical way to identify files (stat.st_ino, etc.).

### 6. Concurrency Model

Xinim is currently single-core Ring 0 (all servers run in kernel mode).
The correct locking model for now is:

- Per-inode ScopedIrqLock for modification (set mode, truncate, write)
- Read-path is lock-free when the inode is not being modified (no writer)

For future SMP support, the per-inode lock becomes a MCSSpinlock (already
implemented in v1.2.0). The ScopedIrqLock wrapping ensures no preemption
by the timer interrupt during critical sections.

### 7. VFS Operations Dispatch

Replace virtual dispatch (VNode with virtual methods) with function pointer
tables per filesystem type:

```cpp
struct FsOps {
    int (*open)  (uint32_t ino, int flags, int* out_fd);
    int (*read)  (uint32_t ino, void* buf, size_t len, off_t off);
    int (*write) (uint32_t ino, const void* buf, size_t len, off_t off);
    int (*close) (int fd);
    int (*stat)  (uint32_t ino, struct KStat* out);
    int (*mkdir) (uint32_t parent_ino, const char* name, uint16_t mode);
    int (*unlink)(uint32_t parent_ino, const char* name);
    int (*readdir)(uint32_t ino, struct DirEntry* buf, int max_entries);
};
```

**WHY function pointer table instead of virtual:** `virtual` requires RTTI
and exception tables which are disabled in the kernel (`-fno-rtti -fno-exceptions`).
A function pointer table achieves the same polymorphism with zero overhead.

### 8. FD Table

```cpp
constexpr int MAX_FDS = 64;

struct FdEntry {
    uint32_t ino;           // Open inode (0 = free)
    uint32_t flags;         // O_RDONLY, O_WRONLY, O_RDWR, O_APPEND
    off_t    pos;           // Current file position
};

static FdEntry g_fd_table[MAX_FDS];
```

No heap allocation. O(MAX_FDS) scan on open to find free slot. For 64 FDs
this is negligible. VFS server maintains one global FD table (per-process
tables are a v1.3.0+ concern; for now root process gets all FDs).

---

## Memory Budget

| Structure | Size |
|-----------|------|
| Inode table (1024 inodes * 64 B) | 64 KB |
| Dirent arena (256 KB, covers ~8192 entries) | 256 KB |
| Data arena (file data, 1 MB) | 1 MB |
| Buffer cache (64 blocks * 512 B) | 32 KB |
| Mount table (8 entries * 64 B) | 512 B |
| FD table (64 entries * 16 B) | 1 KB |
| **Total** | **~1.35 MB** |

This fits comfortably in the 4 MB kernel heap from v1.2.0 Phase 1.

---

## Implementation Plan for v1.3.0

The following tasks decompose ADR-0009 into atomic implementation units:

| # | Task | Files |
|---|------|-------|
| 1 | Define RawInode, DirEntry, MountEntry, FdEntry structs | `src/vfs/bare_vfs.hpp` |
| 2 | Implement inode allocator (flat array + bitmap) | `src/vfs/inode_table.cpp` |
| 3 | Implement dirent_arena: add_entry, lookup, remove_entry | `src/vfs/dirent.cpp` |
| 4 | Implement path walk: iterate components, call lookup at each level | `src/vfs/path_walk.cpp` |
| 5 | Implement ramfs FsOps: open/read/write/close/stat/mkdir/unlink | `src/vfs/ramfs_ops.cpp` |
| 6 | Implement mount table: mount, unmount, longest-prefix resolve | `src/vfs/mount_table.cpp` |
| 7 | Implement FD table: allocate_fd, get_fd, release_fd | `src/vfs/fd_table.cpp` |
| 8 | Implement VFS message loop: handle VFS IPC messages | `src/vfs/vfs_server.cpp` |
| 9 | Wire buffer cache to ramfs_ops (data_arena backed reads/writes) | `src/vfs/buffer_cache.cpp` (rewrite) |
| 10 | Tests: test_bare_vfs.cpp (create, read, write, stat, mkdir, unlink) | `test/test_bare_vfs.cpp` |

**Acceptance criteria for v1.3.0:**
- `/bin`, `/dev`, `/proc`, `/tmp` mount points initialize at boot
- open/read/write/close on a ramfs file completes a round-trip (POSIX PASS)
- stat() returns correct size, mode, mtime
- mkdir/unlink work
- All existing 31 unit tests continue to pass
- Zero STL includes in VFS server code path

---

## Rejected Alternatives

### A: Patch ramfs.hpp to use kernel allocator wrappers

Replace `std::vector<uint8_t>` with a custom vector backed by `heap_alloc()`,
replace `std::string` with a fixed-size string wrapper, replace `std::unordered_map`
with a custom hash map.

**Rejected because:** The amount of infrastructure code required (custom
vector, string, hash map, smart pointer) is larger than a ground-up design
using fixed-size arrays. Ground-up is faster and produces simpler code.

### B: Keep VNode virtual dispatch, remove STL from headers only

Replace `std::string` parameters with `const char*`, replace containers with
`fixed_array<>` wrappers, keep the virtual method table.

**Rejected because:** virtual requires vtable emission which fails with
`-fno-rtti`. Function pointer tables are equivalent and explicit.

### C: Use src/fs/ (legacy MINIX fs) as the implementation

The legacy `src/fs/` directory has a working (if MINIX-heritage) inode-based
filesystem. It could be modernized.

**Rejected because:** `src/fs/` uses MINIX-era `struct buf`, `struct inode`,
`struct super_block` types that conflict with the C++23 type system. The
code uses `PUBLIC`, `PRIVATE`, and other MINIX macros. The effort to modernize
is comparable to ground-up but with added complexity from legacy idioms.

---

## Consequences

**Positive:**
- VFS compiles freestanding in v1.3.0
- First POSIX-passing syscalls (open/read/write/close on ramfs)
- Fixed memory footprint (~1.35 MB, known at compile time)
- No STL in kernel critical path
- Compatible with future SMP (per-inode MCSSpinlock)

**Negative:**
- MAX_FDS=64 limits per-process open files (acceptable for current scope)
- 26-char inline name limit requires overflow table for long filenames (deferred)
- ext2 and tmpfs from existing src/vfs/ remain broken (they are not in kernel build)
- src/vfs/ existing files remain as reference code, not compiled

**Neutral:**
- src/fs/ remains as historical reference for MINIX inode semantics
- v1.4.0+ can add real persistence (ext2 over virtio-blk) using same FsOps dispatch
