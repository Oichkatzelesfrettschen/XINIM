# XINIM Userspace Servers Implementation Plan

**Date**: 2025-11-17
**Version**: 1.0
**Status**: Planning

## Executive Summary

This document provides a **granular, step-by-step implementation plan** for the three core userspace servers that handle syscall delegation in XINIM's microkernel architecture:

1. **VFS Server** - Virtual File System (file I/O syscalls)
2. **Process Manager** - Process lifecycle (fork, exec, wait, kill)
3. **Memory Manager** - Memory allocation (brk, mmap, munmap, mprotect)

**Architecture**: Kernel syscall dispatcher → Lattice IPC → Userspace server → Response

---

## Table of Contents

1. [Current State Analysis](#current-state-analysis)
2. [Gap Identification](#gap-identification)
3. [Message Protocol Design](#message-protocol-design)
4. [VFS Server Implementation](#vfs-server-implementation)
5. [Process Manager Implementation](#process-manager-implementation)
6. [Memory Manager Implementation](#memory-manager-implementation)
7. [Integration Strategy](#integration-strategy)
8. [Testing Strategy](#testing-strategy)
9. [Implementation Order](#implementation-order)
10. [Timeline and Milestones](#timeline-and-milestones)

---

## 1. Current State Analysis

### 1.1 Existing Infrastructure

#### ✅ **Kernel Syscall Dispatcher** (`src/kernel/sys/dispatch.cpp`)
- **Status**: Complete (49 syscalls defined)
- **Implementation**: Stub handlers with TODO comments
- **Working**: `write(1/2)`, `getpid()`, `time()`
- **Delegates**: File I/O, process mgmt, memory mgmt

```cpp
static int64_t sys_open_impl(const char* path, int flags, int mode) {
    // TODO: Send IPC message to VFS server
    return -1; // ENOENT
}
```

#### ✅ **Lattice IPC** (`src/kernel/lattice_ipc.hpp/cpp`)
- **Status**: Complete
- **Features**:
  - DAG-based message routing
  - XChaCha20-Poly1305 encryption per channel
  - Post-quantum key exchange (Kyber/ML-KEM)
  - Blocking and non-blocking semantics
  - Direct yield to receiver optimization

**API**:
```cpp
int lattice_connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id = 0);
int lattice_send(xinim::pid_t src, xinim::pid_t dst, const message &msg, IpcFlags flags);
int lattice_recv(xinim::pid_t pid, message *msg, IpcFlags flags);
```

**Message structure** (`include/sys/type.hpp:109`):
```cpp
struct message {
    int m_source;  // who sent the message
    int m_type;    // what kind of message is it
    union {
        mess_1 m_m1;  // Generic payload
        mess_2 m_m2;  // String-based payload
        mess_3 m_m3;  // Byte array payload
        mess_4 m_m4;  // Long integer payload
        mess_5 m_m5;  // Memory mapping payload
        mess_6 m_m6;  // Function pointer payload
    } m_u;
};
```

#### ✅ **VFS Foundation** (`src/vfs/vfs.cpp`)
- **Status**: Partial (infrastructure only)
- **Implemented**:
  - VNode interface (virtual file node)
  - File descriptors (3+)
  - Path resolution (absolute paths)
  - Mount point management
- **Missing**:
  - No IPC integration
  - No actual filesystem implementation
  - No ramfs/tmpfs
  - No file descriptor table per-process

#### ⚠️ **Process Table** (`src/kernel/process_table.hpp`)
- **Status**: Partial
- **Has**: `ScopedProcessSlot` RAII wrapper
- **Missing**:
  - Global process table
  - Fork/exec implementation
  - Parent-child relationships
  - Zombie process handling

#### ⚠️ **Memory Management** (`src/mm/memory.cpp`)
- **Status**: Partial
- **Has**: Basic allocator
- **Missing**:
  - Per-process heap (brk)
  - Memory mapping (mmap/munmap)
  - Page protection (mprotect)
  - Copy-on-write (for fork)

#### ✅ **dietlibc-XINIM** (`libc/dietlibc-xinim/`)
- **Status**: Complete
- **Syscalls**: Use XINIM numbers (6, 29, etc.)
- **Binary size**: 16 KB (verified)

### 1.2 Current Syscall Flow

**TODAY**:
```
userspace: write(1, "hello", 5)
    ↓ (syscall 6)
kernel: xinim_syscall_dispatch(6, 1, buf, 5, ...)
    ↓ (dispatch.cpp:319)
kernel: sys_write_impl(1, buf, 5)
    ↓ (fd==1 special case)
kernel: sys_debug_write_impl(buf, 5)
    ↓ (early/serial_16550.hpp)
serial: early_serial.write_char(buf[i])
```

**GOAL (for VFS-delegated syscalls)**:
```
userspace: open("/file", O_RDONLY, 0)
    ↓ (syscall 3)
kernel: xinim_syscall_dispatch(3, "/file", O_RDONLY, 0, ...)
    ↓
kernel: sys_open_impl("/file", O_RDONLY, 0)
    ↓ (build IPC message)
kernel: lattice_send(KERNEL_PID, VFS_PID, msg)
    ↓ (lattice IPC)
vfs_server: lattice_recv(VFS_PID, &msg)
    ↓ (process message)
vfs_server: handle_open_request(msg)
    ↓ (return response)
vfs_server: lattice_send(VFS_PID, KERNEL_PID, response)
    ↓
kernel: lattice_recv(KERNEL_PID, &response)
    ↓ (return fd or error)
kernel: return fd to userspace
```

---

## 2. Gap Identification

### 2.1 Critical Gaps

#### Gap 1: **No Server Processes**
- **Impact**: No delegation possible
- **Solution**: Create 3 server executables
- **Files needed**:
  - `src/servers/vfs_server/main.cpp`
  - `src/servers/process_manager/main.cpp`
  - `src/servers/memory_manager/main.cpp`

#### Gap 2: **No Message Protocol**
- **Impact**: No standard request/response format
- **Solution**: Design message types and payloads
- **Files needed**:
  - `include/xinim/ipc/vfs_protocol.hpp`
  - `include/xinim/ipc/process_protocol.hpp`
  - `include/xinim/ipc/memory_protocol.hpp`

#### Gap 3: **No IPC Integration in Syscalls**
- **Impact**: Kernel can't send messages
- **Solution**: Modify `dispatch.cpp` to use `lattice_send/recv`
- **Files modified**:
  - `src/kernel/sys/dispatch.cpp` (380 lines → ~1500 lines)

#### Gap 4: **No Server PIDs**
- **Impact**: Don't know where to send messages
- **Solution**: Define well-known PIDs or service discovery
- **Options**:
  - **Well-known PIDs**: VFS=2, PROCMGR=3, MEMMGR=4
  - **Service registry**: Dynamic PID lookup by name

#### Gap 5: **No Per-Process State**
- **Impact**: Can't track file descriptors, heap, etc.
- **Solution**: Create process control blocks
- **Files needed**:
  - `src/kernel/process_control_block.hpp`
  - Tracks: FD table, heap boundary, memory mappings

#### Gap 6: **No ramfs/tmpfs**
- **Impact**: No filesystem to actually store files
- **Solution**: Implement in-memory filesystem
- **Files needed**:
  - `src/vfs/ramfs.cpp`
  - Basic directory + file support

#### Gap 7: **No Build System Integration**
- **Impact**: Servers won't be compiled
- **Solution**: Update Makefile/CMakeLists
- **Files modified**:
  - `src/servers/Makefile` (new)
  - Top-level build integration

---

## 3. Message Protocol Design

### 3.1 Message Type Enumeration

**File**: `include/xinim/ipc/message_types.h`

```c
#pragma once
#include <stdint.h>

// ============================================================================
// VFS Message Types (100-199)
// ============================================================================
#define VFS_OPEN        100
#define VFS_CLOSE       101
#define VFS_READ        102
#define VFS_WRITE       103
#define VFS_LSEEK       104
#define VFS_STAT        105
#define VFS_FSTAT       106
#define VFS_ACCESS      107
#define VFS_DUP         108
#define VFS_DUP2        109
#define VFS_PIPE        110
#define VFS_IOCTL       111
#define VFS_FCNTL       112
#define VFS_MKDIR       113
#define VFS_RMDIR       114
#define VFS_CHDIR       115
#define VFS_GETCWD      116
#define VFS_LINK        117
#define VFS_UNLINK      118
#define VFS_RENAME      119
#define VFS_CHMOD       120
#define VFS_CHOWN       121

// Response codes
#define VFS_REPLY       150

// ============================================================================
// Process Manager Message Types (200-299)
// ============================================================================
#define PROC_FORK       200
#define PROC_EXEC       201
#define PROC_EXIT       202
#define PROC_WAIT4      203
#define PROC_KILL       204
#define PROC_GETPID     205
#define PROC_GETPPID    206
#define PROC_SIGNAL     207
#define PROC_SIGACTION  208

// Response codes
#define PROC_REPLY      250

// ============================================================================
// Memory Manager Message Types (300-399)
// ============================================================================
#define MM_BRK          300
#define MM_MMAP         301
#define MM_MUNMAP       302
#define MM_MPROTECT     303

// Response codes
#define MM_REPLY        350
```

### 3.2 VFS Protocol Messages

**File**: `include/xinim/ipc/vfs_protocol.hpp`

```cpp
#pragma once
#include <xinim/core_types.hpp>
#include <sys/type.hpp>
#include <fcntl.h>
#include <sys/stat.h>

namespace xinim::ipc {

// ============================================================================
// VFS_OPEN Request
// ============================================================================
struct VfsOpenRequest {
    char path[256];     // File path (absolute)
    int flags;          // O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, etc.
    int mode;           // Permissions for O_CREAT
    pid_t caller_pid;   // Requesting process
};

struct VfsOpenResponse {
    int fd;             // File descriptor (>=0) or error code (<0)
    int error;          // errno value (0 = success)
};

// ============================================================================
// VFS_CLOSE Request
// ============================================================================
struct VfsCloseRequest {
    int fd;             // File descriptor to close
    pid_t caller_pid;   // Requesting process
};

struct VfsCloseResponse {
    int result;         // 0 on success, -errno on error
};

// ============================================================================
// VFS_READ Request
// ============================================================================
struct VfsReadRequest {
    int fd;             // File descriptor
    uint64_t count;     // Number of bytes to read
    pid_t caller_pid;   // Requesting process
};

struct VfsReadResponse {
    int64_t bytes_read; // Number of bytes read (>=0) or -errno
    char data[4096];    // Read buffer (max 4KB per message)
};

// ============================================================================
// VFS_WRITE Request
// ============================================================================
struct VfsWriteRequest {
    int fd;             // File descriptor
    uint64_t count;     // Number of bytes to write
    char data[4096];    // Write buffer (max 4KB per message)
    pid_t caller_pid;   // Requesting process
};

struct VfsWriteResponse {
    int64_t bytes_written; // Number of bytes written (>=0) or -errno
};

// ============================================================================
// VFS_STAT Request
// ============================================================================
struct VfsStatRequest {
    char path[256];     // File path
    pid_t caller_pid;   // Requesting process
};

struct VfsStatResponse {
    int result;         // 0 on success, -errno on error
    struct stat st;     // File attributes
};

// ============================================================================
// Helper Functions
// ============================================================================

// Build VFS_OPEN request message
inline void build_vfs_open_request(message* msg, const char* path,
                                   int flags, int mode, pid_t caller_pid) {
    msg->m_type = VFS_OPEN;
    msg->m_source = caller_pid;

    VfsOpenRequest* req = reinterpret_cast<VfsOpenRequest*>(&msg->m_u.m_m2);
    strncpy(req->path, path, sizeof(req->path) - 1);
    req->path[sizeof(req->path) - 1] = '\0';
    req->flags = flags;
    req->mode = mode;
    req->caller_pid = caller_pid;
}

// Extract VFS_OPEN response
inline VfsOpenResponse extract_vfs_open_response(const message* msg) {
    const VfsOpenResponse* resp = reinterpret_cast<const VfsOpenResponse*>(&msg->m_u.m_m1);
    return *resp;
}

// Similar helpers for READ, WRITE, CLOSE, STAT, etc.

} // namespace xinim::ipc
```

### 3.3 Process Manager Protocol

**File**: `include/xinim/ipc/process_protocol.hpp`

```cpp
#pragma once
#include <xinim/core_types.hpp>
#include <sys/type.hpp>

namespace xinim::ipc {

// ============================================================================
// PROC_FORK Request
// ============================================================================
struct ProcForkRequest {
    pid_t parent_pid;   // Parent process PID
};

struct ProcForkResponse {
    pid_t child_pid;    // Child PID (in parent) or 0 (in child) or -errno
    int error;          // errno value
};

// ============================================================================
// PROC_EXEC Request
// ============================================================================
struct ProcExecRequest {
    char filename[256]; // Executable path
    char argv[1024];    // Arguments (null-separated)
    char envp[1024];    // Environment (null-separated)
    pid_t caller_pid;   // Calling process
};

struct ProcExecResponse {
    int result;         // 0 on success (doesn't return), -errno on error
};

// ============================================================================
// PROC_EXIT Request
// ============================================================================
struct ProcExitRequest {
    int status;         // Exit status
    pid_t caller_pid;   // Exiting process
};

// No response - process terminates

// ============================================================================
// PROC_WAIT4 Request
// ============================================================================
struct ProcWait4Request {
    pid_t wait_pid;     // PID to wait for (-1 = any child)
    int options;        // WNOHANG, WUNTRACED, etc.
    pid_t caller_pid;   // Calling process
};

struct ProcWait4Response {
    pid_t pid;          // PID of child that exited, or -errno
    int status;         // Exit status (via WEXITSTATUS)
};

// ============================================================================
// PROC_KILL Request
// ============================================================================
struct ProcKillRequest {
    pid_t target_pid;   // Process to kill
    int signal;         // Signal number
    pid_t caller_pid;   // Sender
};

struct ProcKillResponse {
    int result;         // 0 on success, -errno on error
};

} // namespace xinim::ipc
```

### 3.4 Memory Manager Protocol

**File**: `include/xinim/ipc/memory_protocol.hpp`

```cpp
#pragma once
#include <xinim/core_types.hpp>
#include <sys/type.hpp>
#include <sys/mman.h>

namespace xinim::ipc {

// ============================================================================
// MM_BRK Request
// ============================================================================
struct MmBrkRequest {
    uint64_t new_brk;   // New heap end (0 = query current)
    pid_t caller_pid;   // Calling process
};

struct MmBrkResponse {
    uint64_t brk;       // Current/new heap end, or -errno
};

// ============================================================================
// MM_MMAP Request
// ============================================================================
struct MmMmapRequest {
    uint64_t addr;      // Requested address (0 = kernel chooses)
    uint64_t length;    // Mapping length
    int prot;           // PROT_READ, PROT_WRITE, PROT_EXEC
    int flags;          // MAP_PRIVATE, MAP_SHARED, MAP_ANONYMOUS, etc.
    int fd;             // File descriptor (or -1 for anonymous)
    int64_t offset;     // File offset
    pid_t caller_pid;   // Calling process
};

struct MmMmapResponse {
    uint64_t addr;      // Mapped address, or -errno
};

// ============================================================================
// MM_MUNMAP Request
// ============================================================================
struct MmMunmapRequest {
    uint64_t addr;      // Start address
    uint64_t length;    // Length to unmap
    pid_t caller_pid;   // Calling process
};

struct MmMunmapResponse {
    int result;         // 0 on success, -errno on error
};

// ============================================================================
// MM_MPROTECT Request
// ============================================================================
struct MmMprotectRequest {
    uint64_t addr;      // Start address
    uint64_t length;    // Length
    int prot;           // New protection flags
    pid_t caller_pid;   // Calling process
};

struct MmMprotectResponse {
    int result;         // 0 on success, -errno on error
};

} // namespace xinim::ipc
```

---

## 4. VFS Server Implementation

### 4.1 Architecture

**Process Model**:
- **PID**: 2 (well-known VFS_SERVER_PID)
- **Main Loop**: Event loop receiving IPC messages
- **State**: Global filesystem tree (ramfs)
- **Concurrency**: Single-threaded (for simplicity)

**File Descriptor Management**:
- Each process has its own FD table
- VFS server maintains per-process FD mapping
- FD → VNode mapping

### 4.2 Implementation Steps

#### Step 1: Create ramfs (In-Memory Filesystem)

**File**: `src/vfs/ramfs.hpp`

```cpp
#pragma once
#include <xinim/vfs/vfs.hpp>
#include <map>
#include <memory>
#include <vector>

namespace xinim::vfs {

class RamFsNode : public VNode {
public:
    RamFsNode(FileType type, uint32_t mode);

    // File operations
    int read(void* buffer, size_t size, uint64_t offset) override;
    int write(const void* buffer, size_t size, uint64_t offset) override;
    int truncate(uint64_t new_size) override;

    // Directory operations
    std::vector<std::string> readdir() override;
    VNode* lookup(const std::string& name) override;
    int create(const std::string& name, uint32_t mode, VNode** out) override;
    int mkdir(const std::string& name, uint32_t mode, VNode** out) override;
    int unlink(const std::string& name) override;
    int rmdir(const std::string& name) override;

private:
    std::vector<uint8_t> data_;  // File content
    std::map<std::string, std::shared_ptr<RamFsNode>> children_;  // Directory entries
};

class RamFs : public FileSystem {
public:
    RamFs();

    int mount(const std::string& device, uint32_t flags) override;
    int unmount() override;
    int sync() override;
    VNode* get_root() override;

private:
    std::shared_ptr<RamFsNode> root_;
};

} // namespace xinim::vfs
```

**Implementation**: `src/vfs/ramfs.cpp` (~400 lines)

**Key features**:
- Files stored as `std::vector<uint8_t>`
- Directories stored as `std::map<std::string, shared_ptr<RamFsNode>>`
- Implements all VNode operations
- No persistence (in-memory only)

**Estimated time**: 6 hours

---

#### Step 2: Create VFS Server Main Loop

**File**: `src/servers/vfs_server/main.cpp`

```cpp
#include <xinim/ipc/vfs_protocol.hpp>
#include <xinim/vfs/vfs.hpp>
#include <xinim/vfs/ramfs.hpp>
#include <kernel/lattice_ipc.hpp>
#include <sys/type.hpp>
#include <cstdio>
#include <unordered_map>

using namespace xinim;
using namespace xinim::vfs;
using namespace xinim::ipc;
using namespace lattice;

// Well-known PID
constexpr pid_t VFS_SERVER_PID = 2;

// Per-process file descriptor tables
struct ProcessFileTable {
    std::unordered_map<int, VNode*> fds;  // fd -> VNode
    int next_fd = 3;  // 0, 1, 2 reserved
};

std::unordered_map<pid_t, ProcessFileTable> process_tables;

// Handler functions
int handle_open(const VfsOpenRequest& req, VfsOpenResponse& resp);
int handle_close(const VfsCloseRequest& req, VfsCloseResponse& resp);
int handle_read(const VfsReadRequest& req, VfsReadResponse& resp);
int handle_write(const VfsWriteRequest& req, VfsWriteResponse& resp);
// ... more handlers

int main() {
    printf("[VFS Server] Starting on PID %d\n", VFS_SERVER_PID);

    // Initialize ramfs
    VFS& vfs = VFS::instance();
    vfs.register_filesystem("ramfs", []() {
        return std::make_unique<RamFs>();
    });

    // Mount root filesystem
    int ret = vfs.mount("", "/", "ramfs", 0);
    if (ret < 0) {
        printf("[VFS Server] Failed to mount root: %d\n", ret);
        return 1;
    }

    // Main event loop
    printf("[VFS Server] Entering main loop...\n");
    message msg;
    while (true) {
        // Receive IPC message
        ret = lattice_recv(VFS_SERVER_PID, &msg, IpcFlags::NONE);
        if (ret != OK) {
            printf("[VFS Server] lattice_recv failed: %d\n", ret);
            continue;
        }

        // Dispatch based on message type
        message response;
        response.m_source = VFS_SERVER_PID;
        response.m_type = VFS_REPLY;

        switch (msg.m_type) {
            case VFS_OPEN: {
                VfsOpenRequest* req = reinterpret_cast<VfsOpenRequest*>(&msg.m_u.m_m2);
                VfsOpenResponse* resp = reinterpret_cast<VfsOpenResponse*>(&response.m_u.m_m1);
                handle_open(*req, *resp);
                break;
            }
            case VFS_READ: {
                VfsReadRequest* req = reinterpret_cast<VfsReadRequest*>(&msg.m_u.m_m1);
                VfsReadResponse* resp = reinterpret_cast<VfsReadResponse*>(&response.m_u.m_m3);
                handle_read(*req, *resp);
                break;
            }
            case VFS_WRITE: {
                VfsWriteRequest* req = reinterpret_cast<VfsWriteRequest*>(&msg.m_u.m_m3);
                VfsWriteResponse* resp = reinterpret_cast<VfsWriteResponse*>(&response.m_u.m_m1);
                handle_write(*req, *resp);
                break;
            }
            case VFS_CLOSE: {
                VfsCloseRequest* req = reinterpret_cast<VfsCloseRequest*>(&msg.m_u.m_m1);
                VfsCloseResponse* resp = reinterpret_cast<VfsCloseResponse*>(&response.m_u.m_m1);
                handle_close(*req, *resp);
                break;
            }
            // ... more cases

            default:
                printf("[VFS Server] Unknown message type: %d\n", msg.m_type);
                continue;
        }

        // Send response back to caller
        lattice_send(VFS_SERVER_PID, msg.m_source, response, IpcFlags::NONE);
    }

    return 0;
}

int handle_open(const VfsOpenRequest& req, VfsOpenResponse& resp) {
    printf("[VFS] OPEN: pid=%d path='%s' flags=0x%x mode=0%o\n",
           req.caller_pid, req.path, req.flags, req.mode);

    VFS& vfs = VFS::instance();
    VNode* vnode = vfs.open(req.path, req.flags, req.mode);

    if (!vnode) {
        resp.fd = -1;
        resp.error = ENOENT;  // File not found
        return -ENOENT;
    }

    // Allocate FD for this process
    ProcessFileTable& table = process_tables[req.caller_pid];
    int fd = table.next_fd++;
    table.fds[fd] = vnode;

    resp.fd = fd;
    resp.error = 0;
    return 0;
}

int handle_read(const VfsReadRequest& req, VfsReadResponse& resp) {
    printf("[VFS] READ: pid=%d fd=%d count=%lu\n",
           req.caller_pid, req.fd, req.count);

    // Find VNode for this FD
    ProcessFileTable& table = process_tables[req.caller_pid];
    auto it = table.fds.find(req.fd);
    if (it == table.fds.end()) {
        resp.bytes_read = -EBADF;  // Bad file descriptor
        return -EBADF;
    }

    VNode* vnode = it->second;

    // Limit read to 4KB (message buffer size)
    size_t to_read = std::min(req.count, sizeof(resp.data));

    // Read from VNode (offset managed internally for now)
    int ret = vnode->read(resp.data, to_read, 0);  // TODO: track file position
    if (ret < 0) {
        resp.bytes_read = ret;
        return ret;
    }

    resp.bytes_read = ret;
    return 0;
}

// ... implement handle_write, handle_close, etc.
```

**Estimated lines**: ~800 lines
**Estimated time**: 12 hours

---

#### Step 3: Modify Kernel Syscall Dispatcher

**File**: `src/kernel/sys/dispatch.cpp` (modify existing)

```cpp
#include <stdint.h>
#include <xinim/sys/syscalls.h>
#include <xinim/ipc/vfs_protocol.hpp>
#include <kernel/lattice_ipc.hpp>
#include "../early/serial_16550.hpp"
#include "../time/monotonic.hpp"

extern xinim::early::Serial16550 early_serial;

// Well-known server PIDs
constexpr xinim::pid_t VFS_SERVER_PID = 2;
constexpr xinim::pid_t PROC_MGR_PID = 3;
constexpr xinim::pid_t MEM_MGR_PID = 4;

// Current process PID (TODO: get from scheduler)
static xinim::pid_t current_pid() {
    return 1;  // Stub: always PID 1 for now
}

// ============================================================================
// File I/O Syscalls (delegated to VFS server via lattice IPC)
// ============================================================================

static int64_t sys_open_impl(const char* path, int flags, int mode) {
    message msg;
    xinim::ipc::build_vfs_open_request(&msg, path, flags, mode, current_pid());

    // Send to VFS server
    int ret = lattice::lattice_send(current_pid(), VFS_SERVER_PID, msg,
                                     lattice::IpcFlags::NONE);
    if (ret != OK) {
        return -EIO;  // I/O error
    }

    // Wait for response
    message response;
    ret = lattice::lattice_recv(current_pid(), &response, lattice::IpcFlags::NONE);
    if (ret != OK) {
        return -EIO;
    }

    // Extract response
    xinim::ipc::VfsOpenResponse resp = xinim::ipc::extract_vfs_open_response(&response);
    if (resp.error != 0) {
        return -resp.error;
    }

    return resp.fd;
}

static int64_t sys_read_impl(int fd, void* buf, uint64_t count) {
    // Special case: stdin (TODO: delegate to terminal driver)
    if (fd == 0) return 0;  // EOF

    // Build READ request
    message msg;
    msg.m_type = VFS_READ;
    msg.m_source = current_pid();

    xinim::ipc::VfsReadRequest* req =
        reinterpret_cast<xinim::ipc::VfsReadRequest*>(&msg.m_u.m_m1);
    req->fd = fd;
    req->count = count;
    req->caller_pid = current_pid();

    // Send to VFS server
    int ret = lattice::lattice_send(current_pid(), VFS_SERVER_PID, msg,
                                     lattice::IpcFlags::NONE);
    if (ret != OK) {
        return -EIO;
    }

    // Wait for response
    message response;
    ret = lattice::lattice_recv(current_pid(), &response, lattice::IpcFlags::NONE);
    if (ret != OK) {
        return -EIO;
    }

    // Extract response
    xinim::ipc::VfsReadResponse* resp =
        reinterpret_cast<xinim::ipc::VfsReadResponse*>(&response.m_u.m_m3);

    if (resp->bytes_read < 0) {
        return resp->bytes_read;  // Error
    }

    // Copy data to user buffer
    memcpy(buf, resp->data, resp->bytes_read);
    return resp->bytes_read;
}

static int64_t sys_write_impl(int fd, const void* buf, uint64_t count) {
    // Special case: stdout/stderr (write to serial)
    if (fd == 1 || fd == 2) {
        return sys_debug_write_impl((const char*)buf, count);
    }

    // Build WRITE request
    message msg;
    msg.m_type = VFS_WRITE;
    msg.m_source = current_pid();

    xinim::ipc::VfsWriteRequest* req =
        reinterpret_cast<xinim::ipc::VfsWriteRequest*>(&msg.m_u.m_m3);
    req->fd = fd;
    req->count = std::min(count, sizeof(req->data));
    req->caller_pid = current_pid();
    memcpy(req->data, buf, req->count);

    // Send to VFS server
    int ret = lattice::lattice_send(current_pid(), VFS_SERVER_PID, msg,
                                     lattice::IpcFlags::NONE);
    if (ret != OK) {
        return -EIO;
    }

    // Wait for response
    message response;
    ret = lattice::lattice_recv(current_pid(), &response, lattice::IpcFlags::NONE);
    if (ret != OK) {
        return -EIO;
    }

    // Extract response
    xinim::ipc::VfsWriteResponse* resp =
        reinterpret_cast<xinim::ipc::VfsWriteResponse*>(&response.m_u.m_m1);

    return resp->bytes_written;
}

static int64_t sys_close_impl(int fd) {
    message msg;
    msg.m_type = VFS_CLOSE;
    msg.m_source = current_pid();

    xinim::ipc::VfsCloseRequest* req =
        reinterpret_cast<xinim::ipc::VfsCloseRequest*>(&msg.m_u.m_m1);
    req->fd = fd;
    req->caller_pid = current_pid();

    int ret = lattice::lattice_send(current_pid(), VFS_SERVER_PID, msg,
                                     lattice::IpcFlags::NONE);
    if (ret != OK) {
        return -EIO;
    }

    message response;
    ret = lattice::lattice_recv(current_pid(), &response, lattice::IpcFlags::NONE);
    if (ret != OK) {
        return -EIO;
    }

    xinim::ipc::VfsCloseResponse* resp =
        reinterpret_cast<xinim::ipc::VfsCloseResponse*>(&response.m_u.m_m1);

    return resp->result;
}

// ... continue for stat, mkdir, etc.
```

**Estimated changes**: +600 lines
**Estimated time**: 8 hours

---

### 4.3 VFS Server Testing

**Test 1: Open/Close**
```c
int fd = open("/test.txt", O_CREAT | O_RDWR, 0644);
assert(fd >= 0);
close(fd);
```

**Test 2: Write/Read**
```c
int fd = open("/test.txt", O_CREAT | O_RDWR, 0644);
write(fd, "Hello", 5);
lseek(fd, 0, SEEK_SET);
char buf[10];
read(fd, buf, 5);
assert(memcmp(buf, "Hello", 5) == 0);
close(fd);
```

**Test 3: Directory Operations**
```c
mkdir("/testdir", 0755);
int fd = open("/testdir/file.txt", O_CREAT | O_RDWR, 0644);
write(fd, "test", 4);
close(fd);
rmdir("/testdir");  // Should fail (not empty)
unlink("/testdir/file.txt");
rmdir("/testdir");  // Should succeed
```

**Estimated time**: 4 hours

---

## 5. Process Manager Implementation

### 5.1 Architecture

**Process Model**:
- **PID**: 3 (well-known PROC_MGR_PID)
- **State**: Global process table
- **Responsibilities**:
  - Fork: Duplicate process
  - Exec: Replace process image
  - Wait: Wait for child termination
  - Kill: Send signals
  - Exit: Terminate process

### 5.2 Process Control Block

**File**: `src/kernel/process_control_block.hpp`

```cpp
#pragma once
#include <xinim/core_types.hpp>
#include <vector>
#include <unordered_map>

namespace xinim {

enum class ProcessState {
    RUNNING,
    SLEEPING,
    ZOMBIE,
    DEAD
};

struct ProcessControlBlock {
    pid_t pid;
    pid_t parent_pid;
    ProcessState state;
    int exit_status;

    // File descriptor table (per-process)
    std::unordered_map<int, int> fd_table;  // local_fd -> global_fd

    // Memory layout
    uint64_t heap_start;
    uint64_t heap_end;  // brk pointer
    std::vector<std::pair<uint64_t, uint64_t>> mappings;  // mmap regions

    // Children
    std::vector<pid_t> children;

    // Executable path
    char executable[256];
};

class ProcessTable {
public:
    static ProcessTable& instance();

    pid_t allocate_pid();
    ProcessControlBlock* get_pcb(pid_t pid);
    int add_process(const ProcessControlBlock& pcb);
    int remove_process(pid_t pid);

    // Parent-child relationships
    std::vector<pid_t> get_children(pid_t parent_pid);
    pid_t get_parent(pid_t pid);

private:
    ProcessTable() = default;

    std::unordered_map<pid_t, ProcessControlBlock> processes_;
    pid_t next_pid_ = 1;
    std::mutex mutex_;
};

} // namespace xinim
```

**Estimated lines**: ~200 lines
**Estimated time**: 3 hours

---

### 5.3 Process Manager Main Loop

**File**: `src/servers/process_manager/main.cpp`

```cpp
#include <xinim/ipc/process_protocol.hpp>
#include <kernel/lattice_ipc.hpp>
#include <kernel/process_control_block.hpp>
#include <sys/type.hpp>
#include <cstdio>

using namespace xinim;
using namespace xinim::ipc;
using namespace lattice;

constexpr pid_t PROC_MGR_PID = 3;

int handle_fork(const ProcForkRequest& req, ProcForkResponse& resp);
int handle_exec(const ProcExecRequest& req, ProcExecResponse& resp);
int handle_exit(const ProcExitRequest& req);
int handle_wait4(const ProcWait4Request& req, ProcWait4Response& resp);
int handle_kill(const ProcKillRequest& req, ProcKillResponse& resp);

int main() {
    printf("[Process Manager] Starting on PID %d\n", PROC_MGR_PID);

    message msg;
    while (true) {
        int ret = lattice_recv(PROC_MGR_PID, &msg, IpcFlags::NONE);
        if (ret != OK) {
            continue;
        }

        message response;
        response.m_source = PROC_MGR_PID;
        response.m_type = PROC_REPLY;

        switch (msg.m_type) {
            case PROC_FORK: {
                ProcForkRequest* req = reinterpret_cast<ProcForkRequest*>(&msg.m_u.m_m1);
                ProcForkResponse* resp = reinterpret_cast<ProcForkResponse*>(&response.m_u.m_m1);
                handle_fork(*req, *resp);
                break;
            }
            case PROC_EXEC: {
                ProcExecRequest* req = reinterpret_cast<ProcExecRequest*>(&msg.m_u.m_m2);
                ProcExecResponse* resp = reinterpret_cast<ProcExecResponse*>(&response.m_u.m_m1);
                handle_exec(*req, *resp);
                break;
            }
            case PROC_EXIT: {
                ProcExitRequest* req = reinterpret_cast<ProcExitRequest*>(&msg.m_u.m_m1);
                handle_exit(*req);
                continue;  // No response for exit
            }
            case PROC_WAIT4: {
                ProcWait4Request* req = reinterpret_cast<ProcWait4Request*>(&msg.m_u.m_m1);
                ProcWait4Response* resp = reinterpret_cast<ProcWait4Response*>(&response.m_u.m_m1);
                handle_wait4(*req, *resp);
                break;
            }
            case PROC_KILL: {
                ProcKillRequest* req = reinterpret_cast<ProcKillRequest*>(&msg.m_u.m_m1);
                ProcKillResponse* resp = reinterpret_cast<ProcKillResponse*>(&response.m_u.m_m1);
                handle_kill(*req, *resp);
                break;
            }
            default:
                printf("[Process Manager] Unknown message type: %d\n", msg.m_type);
                continue;
        }

        lattice_send(PROC_MGR_PID, msg.m_source, response, IpcFlags::NONE);
    }

    return 0;
}

int handle_fork(const ProcForkRequest& req, ProcForkResponse& resp) {
    printf("[PROCMGR] FORK: parent=%d\n", req.parent_pid);

    ProcessTable& table = ProcessTable::instance();
    ProcessControlBlock* parent_pcb = table.get_pcb(req.parent_pid);
    if (!parent_pcb) {
        resp.child_pid = -ESRCH;
        resp.error = ESRCH;
        return -ESRCH;
    }

    // Allocate new PID
    pid_t child_pid = table.allocate_pid();

    // Duplicate parent PCB
    ProcessControlBlock child_pcb = *parent_pcb;
    child_pcb.pid = child_pid;
    child_pcb.parent_pid = req.parent_pid;
    child_pcb.state = ProcessState::RUNNING;
    child_pcb.children.clear();

    // TODO: Copy memory (implement COW)

    // Add child to process table
    table.add_process(child_pcb);

    // Add child to parent's children list
    parent_pcb->children.push_back(child_pid);

    resp.child_pid = child_pid;
    resp.error = 0;
    return 0;
}

int handle_wait4(const ProcWait4Request& req, ProcWait4Response& resp) {
    printf("[PROCMGR] WAIT4: caller=%d wait_pid=%d\n", req.caller_pid, req.wait_pid);

    ProcessTable& table = ProcessTable::instance();
    ProcessControlBlock* caller_pcb = table.get_pcb(req.caller_pid);
    if (!caller_pcb) {
        resp.pid = -ESRCH;
        return -ESRCH;
    }

    // Find zombie child
    for (pid_t child_pid : caller_pcb->children) {
        ProcessControlBlock* child_pcb = table.get_pcb(child_pid);
        if (!child_pcb) continue;

        if (req.wait_pid == -1 || req.wait_pid == child_pid) {
            if (child_pcb->state == ProcessState::ZOMBIE) {
                resp.pid = child_pid;
                resp.status = child_pcb->exit_status;

                // Reap zombie
                table.remove_process(child_pid);

                // Remove from parent's children list
                auto it = std::find(caller_pcb->children.begin(),
                                   caller_pcb->children.end(), child_pid);
                if (it != caller_pcb->children.end()) {
                    caller_pcb->children.erase(it);
                }

                return 0;
            }
        }
    }

    // No zombie found
    if (req.options & WNOHANG) {
        resp.pid = 0;  // Non-blocking, no child ready
        return 0;
    }

    // TODO: Block until child exits (implement sleeping/wakeup)
    resp.pid = -ECHILD;
    return -ECHILD;
}

int handle_exit(const ProcExitRequest& req) {
    printf("[PROCMGR] EXIT: pid=%d status=%d\n", req.caller_pid, req.status);

    ProcessTable& table = ProcessTable::instance();
    ProcessControlBlock* pcb = table.get_pcb(req.caller_pid);
    if (!pcb) {
        return -ESRCH;
    }

    // Mark as zombie
    pcb->state = ProcessState::ZOMBIE;
    pcb->exit_status = req.status;

    // TODO: Notify parent (wake up if waiting)

    return 0;
}

// ... implement handle_exec, handle_kill
```

**Estimated lines**: ~600 lines
**Estimated time**: 10 hours

---

### 5.4 Kernel Integration

Modify `dispatch.cpp` to delegate process syscalls:

```cpp
static int64_t sys_fork_impl() {
    message msg;
    msg.m_type = PROC_FORK;
    msg.m_source = current_pid();

    ProcForkRequest* req = reinterpret_cast<ProcForkRequest*>(&msg.m_u.m_m1);
    req->parent_pid = current_pid();

    lattice::lattice_send(current_pid(), PROC_MGR_PID, msg, lattice::IpcFlags::NONE);

    message response;
    lattice::lattice_recv(current_pid(), &response, lattice::IpcFlags::NONE);

    ProcForkResponse* resp = reinterpret_cast<ProcForkResponse*>(&response.m_u.m_m1);
    if (resp->error != 0) {
        return -resp->error;
    }

    return resp->child_pid;
}

// Similar for exit, wait4, kill, etc.
```

**Estimated time**: 6 hours

---

## 6. Memory Manager Implementation

### 6.1 Architecture

**Memory Model**:
- **PID**: 4 (well-known MEM_MGR_PID)
- **Per-Process State**:
  - Heap: `heap_start` → `heap_end` (brk)
  - Mappings: List of (addr, length, prot, flags)
- **Page Allocator**: Buddy allocator or bitmap

### 6.2 Memory Manager Main Loop

**File**: `src/servers/memory_manager/main.cpp`

```cpp
#include <xinim/ipc/memory_protocol.hpp>
#include <kernel/lattice_ipc.hpp>
#include <kernel/process_control_block.hpp>
#include <sys/type.hpp>
#include <cstdio>

using namespace xinim;
using namespace xinim::ipc;
using namespace lattice;

constexpr pid_t MEM_MGR_PID = 4;

int handle_brk(const MmBrkRequest& req, MmBrkResponse& resp);
int handle_mmap(const MmMmapRequest& req, MmMmapResponse& resp);
int handle_munmap(const MmMunmapRequest& req, MmMunmapResponse& resp);
int handle_mprotect(const MmMprotectRequest& req, MmMprotectResponse& resp);

int main() {
    printf("[Memory Manager] Starting on PID %d\n", MEM_MGR_PID);

    message msg;
    while (true) {
        int ret = lattice_recv(MEM_MGR_PID, &msg, IpcFlags::NONE);
        if (ret != OK) {
            continue;
        }

        message response;
        response.m_source = MEM_MGR_PID;
        response.m_type = MM_REPLY;

        switch (msg.m_type) {
            case MM_BRK: {
                MmBrkRequest* req = reinterpret_cast<MmBrkRequest*>(&msg.m_u.m_m4);
                MmBrkResponse* resp = reinterpret_cast<MmBrkResponse*>(&msg.m_u.m_m4);
                handle_brk(*req, *resp);
                break;
            }
            case MM_MMAP: {
                MmMmapRequest* req = reinterpret_cast<MmMmapRequest*>(&msg.m_u.m_m5);
                MmMmapResponse* resp = reinterpret_cast<MmMmapResponse*>(&msg.m_u.m_m4);
                handle_mmap(*req, *resp);
                break;
            }
            case MM_MUNMAP: {
                MmMunmapRequest* req = reinterpret_cast<MmMunmapRequest*>(&msg.m_u.m_m4);
                MmMunmapResponse* resp = reinterpret_cast<MmMunmapResponse*>(&msg.m_u.m_m1);
                handle_munmap(*req, *resp);
                break;
            }
            case MM_MPROTECT: {
                MmMprotectRequest* req = reinterpret_cast<MmMprotectRequest*>(&msg.m_u.m_m4);
                MmMprotectResponse* resp = reinterpret_cast<MmMprotectResponse*>(&msg.m_u.m_m1);
                handle_mprotect(*req, *resp);
                break;
            }
            default:
                printf("[Memory Manager] Unknown message type: %d\n", msg.m_type);
                continue;
        }

        lattice_send(MEM_MGR_PID, msg.m_source, response, IpcFlags::NONE);
    }

    return 0;
}

int handle_brk(const MmBrkRequest& req, MmBrkResponse& resp) {
    printf("[MEMMGR] BRK: pid=%d new_brk=0x%lx\n", req.caller_pid, req.new_brk);

    ProcessTable& table = ProcessTable::instance();
    ProcessControlBlock* pcb = table.get_pcb(req.caller_pid);
    if (!pcb) {
        resp.brk = -ESRCH;
        return -ESRCH;
    }

    // Query current brk
    if (req.new_brk == 0) {
        resp.brk = pcb->heap_end;
        return 0;
    }

    // Validate new brk
    if (req.new_brk < pcb->heap_start) {
        resp.brk = pcb->heap_end;  // Don't change
        return -EINVAL;
    }

    // TODO: Actually allocate/deallocate pages

    // Update heap end
    pcb->heap_end = req.new_brk;
    resp.brk = pcb->heap_end;
    return 0;
}

int handle_mmap(const MmMmapRequest& req, MmMmapResponse& resp) {
    printf("[MEMMGR] MMAP: pid=%d addr=0x%lx length=%lu prot=0x%x flags=0x%x\n",
           req.caller_pid, req.addr, req.length, req.prot, req.flags);

    ProcessTable& table = ProcessTable::instance();
    ProcessControlBlock* pcb = table.get_pcb(req.caller_pid);
    if (!pcb) {
        resp.addr = -ESRCH;
        return -ESRCH;
    }

    // TODO: Find free region
    uint64_t allocated_addr = 0x40000000;  // Stub: fixed address

    // Add mapping to PCB
    pcb->mappings.push_back({allocated_addr, req.length});

    resp.addr = allocated_addr;
    return 0;
}

// ... implement handle_munmap, handle_mprotect
```

**Estimated lines**: ~400 lines
**Estimated time**: 8 hours

---

## 7. Integration Strategy

### 7.1 Build System Updates

**File**: `src/servers/Makefile`

```makefile
CXX = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra
INCLUDES = -I../../include -I..

all: vfs_server process_manager memory_manager

vfs_server: vfs_server/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ -lpthread

process_manager: process_manager/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ -lpthread

memory_manager: memory_manager/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ -lpthread

clean:
	rm -f vfs_server process_manager memory_manager

.PHONY: all clean
```

**Estimated time**: 1 hour

---

### 7.2 Boot Sequence

1. **Kernel boots** → mounts initramfs
2. **Start VFS server** (PID 2)
3. **Start Process Manager** (PID 3)
4. **Start Memory Manager** (PID 4)
5. **Kernel establishes IPC channels**:
   - `lattice_connect(KERNEL_PID, VFS_SERVER_PID)`
   - `lattice_connect(KERNEL_PID, PROC_MGR_PID)`
   - `lattice_connect(KERNEL_PID, MEM_MGR_PID)`
6. **Init process** (PID 1) starts
7. **Userspace ready**

**File**: `src/kernel/init.cpp` (new)

```cpp
void start_servers() {
    // Fork VFS server
    pid_t vfs_pid = spawn_server("/bin/vfs_server");
    assert(vfs_pid == VFS_SERVER_PID);

    // Fork Process Manager
    pid_t proc_pid = spawn_server("/bin/process_manager");
    assert(proc_pid == PROC_MGR_PID);

    // Fork Memory Manager
    pid_t mem_pid = spawn_server("/bin/memory_manager");
    assert(mem_pid == MEM_MGR_PID);

    // Establish IPC channels
    lattice_connect(KERNEL_PID, VFS_SERVER_PID);
    lattice_connect(KERNEL_PID, PROC_MGR_PID);
    lattice_connect(KERNEL_PID, MEM_MGR_PID);

    printf("[Kernel] All servers started and connected\n");
}
```

**Estimated time**: 3 hours

---

## 8. Testing Strategy

### 8.1 Unit Tests

**VFS Server Tests**:
- Test 1: Open non-existent file (should fail)
- Test 2: Create file with O_CREAT
- Test 3: Write 10 bytes, read 10 bytes
- Test 4: mkdir/rmdir
- Test 5: File descriptor isolation per process

**Process Manager Tests**:
- Test 1: Fork creates new process
- Test 2: Exit transitions to zombie
- Test 3: Wait reaps zombie
- Test 4: Kill sends signal

**Memory Manager Tests**:
- Test 1: brk(0) returns current heap
- Test 2: brk(addr) extends heap
- Test 3: mmap allocates region
- Test 4: munmap frees region

### 8.2 Integration Tests

**Test 1: Full File I/O**
```c
// In userspace
int fd = open("/test.txt", O_CREAT | O_RDWR, 0644);
write(fd, "Hello XINIM", 11);
lseek(fd, 0, SEEK_SET);
char buf[20];
int n = read(fd, buf, 11);
assert(n == 11);
assert(memcmp(buf, "Hello XINIM", 11) == 0);
close(fd);
```

**Test 2: Fork/Exec/Wait**
```c
pid_t pid = fork();
if (pid == 0) {
    // Child
    execve("/bin/echo", {"echo", "hello"}, {});
} else {
    // Parent
    int status;
    wait4(pid, &status, 0, NULL);
    printf("Child exited with status %d\n", WEXITSTATUS(status));
}
```

**Test 3: Memory Allocation**
```c
void* heap = sbrk(0);
sbrk(4096);  // Extend heap by 4KB
void* new_heap = sbrk(0);
assert(new_heap == heap + 4096);

void* mapping = mmap(NULL, 8192, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
assert(mapping != MAP_FAILED);
munmap(mapping, 8192);
```

**Estimated time**: 12 hours

---

## 9. Implementation Order

### Phase 1: VFS Server (Week 3-4)
**Priority**: CRITICAL (needed for all file I/O)

| Step | Task | Lines | Time |
|------|------|-------|------|
| 1 | Message protocol design | 400 | 4h |
| 2 | ramfs implementation | 400 | 6h |
| 3 | VFS server main loop | 800 | 12h |
| 4 | Kernel syscall integration | 600 | 8h |
| 5 | Unit tests | 300 | 4h |
| 6 | Integration tests | 200 | 4h |
| **Total** | | **2700** | **38h** |

### Phase 2: Process Manager (Week 5-6)
**Priority**: HIGH (needed for fork/exec)

| Step | Task | Lines | Time |
|------|------|-------|------|
| 1 | Process control block | 200 | 3h |
| 2 | Process table | 200 | 3h |
| 3 | Process manager main loop | 600 | 10h |
| 4 | Kernel syscall integration | 400 | 6h |
| 5 | Unit tests | 200 | 3h |
| 6 | Integration tests | 200 | 3h |
| **Total** | | **1800** | **28h** |

### Phase 3: Memory Manager (Week 7-8)
**Priority**: MEDIUM (needed for malloc)

| Step | Task | Lines | Time |
|------|------|-------|------|
| 1 | Memory manager main loop | 400 | 8h |
| 2 | Kernel syscall integration | 300 | 5h |
| 3 | Page allocator | 500 | 10h |
| 4 | Unit tests | 150 | 2h |
| 5 | Integration tests | 150 | 2h |
| **Total** | | **1500** | **27h** |

### Phase 4: Boot Integration (Week 9)
**Priority**: HIGH (needed to actually run)

| Step | Task | Lines | Time |
|------|------|-------|------|
| 1 | Server spawn mechanism | 200 | 3h |
| 2 | Boot sequence | 150 | 3h |
| 3 | Build system integration | 100 | 2h |
| 4 | End-to-end tests | 300 | 6h |
| **Total** | | **750** | **14h** |

---

## 10. Timeline and Milestones

### Week 3-4: VFS Server
- **Day 1-2**: Message protocol + ramfs (10h)
- **Day 3-5**: VFS server main loop (12h)
- **Day 6-7**: Kernel integration (8h)
- **Day 8**: Testing (4h)
- **Milestone**: `open("/file")` works end-to-end ✅

### Week 5-6: Process Manager
- **Day 1-2**: PCB + Process table (6h)
- **Day 3-4**: Process manager server (10h)
- **Day 5-6**: Kernel integration (6h)
- **Day 7**: Testing (6h)
- **Milestone**: `fork()` creates child process ✅

### Week 7-8: Memory Manager
- **Day 1-3**: Memory manager + page allocator (18h)
- **Day 4-5**: Kernel integration (5h)
- **Day 6**: Testing (4h)
- **Milestone**: `malloc()` works ✅

### Week 9: Integration
- **Day 1-2**: Boot sequence (6h)
- **Day 3-4**: Build system (2h)
- **Day 5-7**: End-to-end testing (6h)
- **Milestone**: Boot XINIM + run userspace programs ✅

---

## 11. Success Criteria

### Minimum Viable Product (MVP)
✅ VFS server handles: open, close, read, write
✅ Process manager handles: fork, exit, wait
✅ Memory manager handles: brk, mmap
✅ Test program boots and runs:
```c
int main() {
    int fd = open("/hello.txt", O_CREAT | O_RDWR, 0644);
    write(fd, "Hello XINIM!\n", 13);
    close(fd);
    printf("Success!\n");
    return 0;
}
```

### Full POSIX Compliance (Future)
- All 49 syscalls implemented
- mksh boots
- coreutils work (cat, ls, cp, etc.)
- POSIX test suite passes

---

## 12. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| IPC performance bottleneck | Medium | High | Optimize message size, add batching |
| Message protocol complexity | Low | Medium | Keep simple, iterate |
| COW fork too hard | High | High | Defer COW, do full copy first |
| Deadlock in IPC | Medium | High | Careful ordering, timeouts |
| Memory leaks in servers | Medium | Medium | Valgrind, RAII |

---

## 13. Next Steps

### Immediate Actions
1. ✅ **Review this plan** - Get approval
2. **Create protocol headers** - Start with VFS
3. **Implement ramfs** - Simple in-memory FS
4. **Build VFS server** - Main event loop
5. **Test** - Verify open/read/write works

### Long-term Goals
- Week 16: Full POSIX.1-2017 compliance
- Boot mksh in QEMU
- Run coreutils
- Pass POSIX test suite

---

## Appendix A: File Inventory

**New Files** (to be created):
```
include/xinim/ipc/
  ├── message_types.h
  ├── vfs_protocol.hpp
  ├── process_protocol.hpp
  └── memory_protocol.hpp

src/kernel/
  ├── process_control_block.hpp
  ├── process_control_block.cpp
  └── init.cpp

src/vfs/
  ├── ramfs.hpp
  └── ramfs.cpp

src/servers/
  ├── Makefile
  ├── vfs_server/main.cpp
  ├── process_manager/main.cpp
  └── memory_manager/main.cpp

test/
  ├── vfs_test.cpp
  ├── process_test.cpp
  └── memory_test.cpp
```

**Modified Files**:
```
src/kernel/sys/dispatch.cpp      (+600 lines)
src/kernel/lattice_ipc.hpp       (maybe +50 lines)
src/vfs/vfs.cpp                  (+200 lines)
Makefile or CMakeLists.txt       (+50 lines)
```

**Total Estimate**:
- **New code**: ~6750 lines
- **Modified code**: ~850 lines
- **Total**: ~7600 lines
- **Time**: ~107 hours (~3 weeks of full-time work)

---

**End of Implementation Plan**

This plan provides a granular, step-by-step roadmap for implementing the three core userspace servers. Each phase is broken down into actionable tasks with time estimates. The architecture is based on XINIM's existing lattice IPC infrastructure and follows microkernel best practices.
