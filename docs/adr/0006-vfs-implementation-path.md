# ADR 0006: VFS Implementation Path

**Date**: 2026-02-26
**Status**: Accepted
**Deciders**: Xinim development team

## Context

Xinim has two VFS implementations:

1. **Legacy `src/fs/`**: MINIX heritage C-style filesystem. Implements MINIX V1 on-disk
   format, inode operations, buffer cache, pipes, path resolution. Not currently
   integrated into the CMake build (not in `add_executable(xinim)`).

2. **New `src/vfs/` + `src/servers/vfs_server/`**: C++23 server-based ramfs. Implements
   an in-memory filesystem via a server that receives IPC messages. Wired into the
   lattice IPC dispatch path (SYS_write, SYS_read, SYS_open, SYS_close route to PID 2).

The question is: which path to invest in for Phase 7 VFS implementation?

## Decision

**Use `src/servers/vfs_server/` + `src/vfs/` as the primary VFS path.**

Rationale:
- The server-based path aligns with the microkernel architecture (VFS as a server).
- It is already wired into the IPC dispatch path.
- The ramfs is suitable for Phase 7 goals (open/read/write on in-memory files).
- The legacy `src/fs/` MINIX V1 code targets a different on-disk format and would
  require substantial adaptation to work as an IPC server.

## Legacy `src/fs/` Disposition

The legacy `src/fs/` directory is **retained as a reference** for:
- MINIX V1 on-disk format understanding (super.cpp, inode.cpp, path.cpp)
- Buffer cache design patterns
- Historical context

It is NOT compiled into the kernel build. It will be archived to `archive/legacy/src/fs/`
in a future cleanup pass once the VFS server handles all Phase 7 syscalls.

## ADR for Ring 3 Migration

A follow-on ADR (0009, Phase 7) will document the Ring 3 transition plan that
moves VFS server from Ring 0 to userspace.

## Consequences

- Phase 7 VFS work targets `src/servers/vfs_server/main.cpp` and `src/vfs/`.
- `src/fs/compat_tests.cpp` remains registered as `test_fs_compat` if buildable.
- Legacy `src/fs/` is not touched in Phase 7 unless needed for reference.
