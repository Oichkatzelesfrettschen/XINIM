# Xinim IPC Protocol

Phase 6 update: 2026-02-26.

## Overview

Xinim uses lattice-based IPC (src/kernel/lattice_ipc.cpp) for kernel-to-server and
server-to-server communication. Capabilities form a lattice ordered by privilege
level. A send is allowed when the sender's capability dominates the receiver's.

## Message Format

The `message` struct is defined in `include/sys/type.hpp` (MINIX heritage):

```c
struct message {
    int m_type;         /* Message type (syscall number or server protocol) */
    union {
        struct { int m1i1, m1i2, m1i3; char *m1p1, *m1p2, *m1p3; } m_m1;
        struct { int m2i1, m2i2, m2i3; long m2l1, m2l2; char *m2p1; } m_m2;
        /* ... additional message formats ... */
    } m_u;
};
```

Messages are carried in a fixed-size queue per channel (QUEUE_SIZE = 8).

## Channel Model

```
src/kernel/lattice_ipc.hpp: class Graph
  - MAX_CHANNELS = 128
  - MAX_PROCS    = 64
  - inbox_[MAX_PROCS]: direct handoff inbox per PID
  - channels[]: directional channel array (src, dst, node_id, queue)
```

`lattice_send(src, dst, msg, flags)` -- enqueues message or writes to inbox.
`lattice_recv(pid, msg*, flags)` -- dequeues from inbox.

## Server Endpoints

| Server | PID | Message Types Handled |
|--------|-----|-----------------------|
| VFS | 2 | SYS_open(2), SYS_read(0), SYS_write(1), SYS_close(3), SYS_lseek(8) |
| PM | 3 | SYS_fork(57), SYS_execve(59), SYS_wait4(61), SYS_kill(62) |
| MM | 4 | SYS_brk(12), SYS_mmap(9), SYS_munmap(11) -- stub |

## Syscall Routing Flow

```
Syscall -> dispatch.cpp -> route_to_server(caller, server_pid, msg_type, a0, a1, a2)
             -> lattice_send(caller, server)
             -> [server processes in Ring 0 loop]
             -> lattice_send(server, caller, reply)
             -> dispatch returns reply m_type to caller
```

## Current Limitations

1. **Ring 0 only**: All IPC happens in kernel mode. No blocking/scheduling
   across send/recv; servers must respond before the kernel tick resumes.
2. **No flow control**: QUEUE_SIZE=8 per channel. Overflow silently drops.
3. **PID is hardcoded**: `caller = 1` in dispatch.cpp (TODO Phase 6).
4. **No capability enforcement**: Capability lattice defined but not checked
   on every message in current code.

## IPC Protocol Headers

- `include/xinim/ipc/message_types.h` -- server PIDs, message type constants
- `include/xinim/ipc/vfs_protocol.hpp` -- VFS-specific message formats
- `include/xinim/ipc/proc_protocol.hpp` -- PM-specific message formats
- `include/xinim/ipc/mm_protocol.hpp` -- MM-specific message formats

## See Also

- `src/kernel/lattice_ipc.hpp` -- Channel and Graph types
- `src/kernel/lattice_ipc.cpp` -- IPC implementation
- `src/kernel/sys/dispatch.cpp` -- Syscall routing
- `docs/adr/0005-lattice-ipc.md` -- Design decision record
- `docs/specs/MICROKERNEL_BOUNDARY.md` -- Kernel vs server responsibilities
