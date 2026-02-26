# Xinim IPC Protocol

Status: Skeleton (Phase 6 will fill in)

## Overview

Xinim uses lattice-based IPC (src/kernel/lattice_ipc.cpp) for kernel-server communication.
Capabilities form a lattice ordered by privilege level. Sends are allowed only when
sender's capability dominates receiver's in the lattice order.

## Message Format

```cpp
struct Message {
    uint64_t type;      // Message type (SYS_open, SYS_read, etc.)
    uint64_t args[6];   // Arguments (syscall-number-dependent)
    uint64_t sender;    // Sender PID
    uint64_t cap;       // Capability token
};
```

## Server Endpoints

| Server | Message Types (planned) |
|--------|------------------------|
| VFS    | SYS_open, SYS_read, SYS_write, SYS_close, SYS_lseek, SYS_stat |
| PM     | SYS_fork, SYS_exit, SYS_wait4, SYS_kill, SYS_getpid |
| MM     | SYS_brk, SYS_mmap, SYS_munmap |

## Current State

- Message queue implemented per-process (spinlock-protected)
- No Ring 3 transition yet; servers run in Ring 0
- SYS_write -> serial output functional

## See Also

- `src/kernel/lattice_ipc.hpp` - Message types and lattice definitions
- `src/kernel/lattice_ipc.cpp` - IPC implementation
- `docs/adr/0005-lattice-ipc.md` - Design decision record
