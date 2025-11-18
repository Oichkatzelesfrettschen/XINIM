# Week 9 Phase 1: VFS Integration with Syscalls

**Status:** 📋 **PLANNING**
**Date:** November 18, 2025
**Milestone:** Integrate syscalls with real VFS layer for file operations

---

## Executive Summary

Week 9 Phase 1 builds upon the syscall infrastructure from Week 8 Phase 4 by integrating syscalls with the actual Virtual File System (VFS) layer. This enables real file operations from Ring 3 userspace processes, including opening files, reading/writing data, and managing file descriptors.

**Goal:** Transform syscalls from simple serial console output to production-quality VFS operations with proper security, validation, and POSIX compliance.

---

## Prerequisites (Completed in Week 8)

- ✅ Fast syscall/sysret mechanism operational
- ✅ Basic syscalls implemented (write, getpid, exit)
- ✅ Ring 3 servers running and can invoke syscalls
- ✅ Syscall dispatch table infrastructure
- ✅ Scheduler with preemptive multitasking

---

## Week 9 Phase 1 Objectives

### Primary Goals

1. **User Pointer Validation** - Secure kernel from malicious user pointers
2. **File Descriptor Tables** - Per-process FD management
3. **VFS Integration** - Connect syscalls to actual VFS layer
4. **Expanded Syscalls** - Implement read, open, close, lseek
5. **Security Hardening** - Permission checking and capability validation

### Success Criteria

- [ ] User can open `/dev/console` and read/write
- [ ] File descriptor table properly managed per-process
- [ ] User pointers validated before dereferencing
- [ ] All basic file operations working from Ring 3
- [ ] No kernel crashes from malicious user input

---

## Technical Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Ring 3 Userspace                          │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ User Process (e.g., shell)                           │   │
│  │   fd = open("/dev/console", O_RDWR);                 │   │
│  │   write(fd, "Hello", 5);                             │   │
│  │   read(fd, buf, 100);                                │   │
│  │   close(fd);                                         │   │
│  └──────────────────────────────────────────────────────┘   │
│                          ↓ syscall                           │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│                    Ring 0 Kernel                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Syscall Handler (syscall_handler.S)                  │   │
│  │   - Stack switching                                  │   │
│  │   - Register save/restore                            │   │
│  └──────────────────────────────────────────────────────┘   │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Syscall Dispatch (syscall_table.cpp)                 │   │
│  │   - Validate syscall number                          │   │
│  │   - Call appropriate handler                         │   │
│  └──────────────────────────────────────────────────────┘   │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ User Pointer Validation (uaccess.cpp) ← NEW          │   │
│  │   - is_user_address()                                │   │
│  │   - copy_from_user()                                 │   │
│  │   - copy_to_user()                                   │   │
│  └──────────────────────────────────────────────────────┘   │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ File Descriptor Layer (fd_table.cpp) ← NEW           │   │
│  │   - Allocate FD                                      │   │
│  │   - Lookup FD → inode                                │   │
│  │   - Close FD                                         │   │
│  └──────────────────────────────────────────────────────┘   │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ VFS Layer (vfs.cpp)                                  │   │
│  │   - Path resolution (/dev/console → inode)          │   │
│  │   - Permission checking                              │   │
│  │   - Inode operations                                 │   │
│  └──────────────────────────────────────────────────────┘   │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Device Drivers                                       │   │
│  │   - /dev/console → serial console                    │   │
│  │   - /dev/null                                        │   │
│  │   - /dev/zero                                        │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 1 Implementation Plan

### Step 1: User Pointer Validation (Security Foundation)

**Why First?** Security must come before functionality. We cannot trust user pointers.

#### Files to Create

1. **src/kernel/uaccess.hpp** (~80 LOC)
2. **src/kernel/uaccess.cpp** (~150 LOC)

#### Key Functions

```cpp
namespace xinim::kernel {

/**
 * @brief Check if address is valid user space address
 *
 * @param addr User space address to check
 * @param size Size of memory region
 * @return true if valid user address, false otherwise
 */
bool is_user_address(uintptr_t addr, size_t size);

/**
 * @brief Safely copy data from user space to kernel space
 *
 * @param dest Kernel destination buffer
 * @param src User source address
 * @param size Number of bytes to copy
 * @return 0 on success, -EFAULT if user address is invalid
 */
int copy_from_user(void* dest, uintptr_t src, size_t size);

/**
 * @brief Safely copy data from kernel space to user space
 *
 * @param dest User destination address
 * @param src Kernel source buffer
 * @param size Number of bytes to copy
 * @return 0 on success, -EFAULT if user address is invalid
 */
int copy_to_user(uintptr_t dest, const void* src, size_t size);

/**
 * @brief Safely copy string from user space (with max length)
 *
 * @param dest Kernel destination buffer
 * @param src User source address (null-terminated string)
 * @param max_len Maximum bytes to copy (including null terminator)
 * @return 0 on success, -EFAULT if invalid, -ENAMETOOLONG if too long
 */
int copy_string_from_user(char* dest, uintptr_t src, size_t max_len);

} // namespace xinim::kernel
```

#### Memory Layout (x86_64)

```
0x0000000000000000  ┌─────────────────────────────┐
                    │   Null Page (unmapped)      │
0x0000000000001000  ├─────────────────────────────┤
                    │                             │
                    │   User Space                │
                    │   (0x1000 - 0x7FFFFFFFFFFF) │
                    │                             │
0x0000800000000000  ├─────────────────────────────┤ ← User/Kernel boundary
                    │   Non-canonical addresses   │
                    │   (holes)                   │
0xFFFF800000000000  ├─────────────────────────────┤
                    │                             │
                    │   Kernel Space              │
                    │   (0xFFFF800000000000 - )   │
                    │                             │
0xFFFFFFFFFFFFFFFF  └─────────────────────────────┘
```

**Validation Rules:**
- User addresses: `0x1000` ≤ addr < `0x0000800000000000`
- Kernel addresses: `0xFFFF800000000000` ≤ addr
- Must not cross user/kernel boundary
- Must be mapped in process page table

#### Implementation Strategy

```cpp
bool is_user_address(uintptr_t addr, size_t size) {
    // Check for null pointer
    if (addr < 0x1000) return false;

    // Check upper bound (must be in user space)
    if (addr >= USER_SPACE_END) return false;

    // Check for overflow
    uintptr_t end = addr + size;
    if (end < addr) return false;  // Overflow
    if (end >= USER_SPACE_END) return false;

    // TODO Week 9: Check if pages are actually mapped in page table

    return true;
}

int copy_from_user(void* dest, uintptr_t src, size_t size) {
    if (!is_user_address(src, size)) {
        return -EFAULT;
    }

    // Use memcpy with page fault handler
    // TODO Week 9: Set up page fault handler to catch invalid accesses
    memcpy(dest, (const void*)src, size);

    return 0;
}
```

---

### Step 2: File Descriptor Tables (Process State Management)

**Purpose:** Each process needs its own table of open files.

#### Files to Create

1. **src/kernel/fd_table.hpp** (~120 LOC)
2. **src/kernel/fd_table.cpp** (~250 LOC)

#### Data Structures

```cpp
namespace xinim::kernel {

/**
 * @brief File descriptor flags
 */
enum class FdFlags : uint32_t {
    NONE        = 0,
    CLOEXEC     = (1 << 0),  ///< Close on exec
};

/**
 * @brief File status flags (from open())
 */
enum class FileFlags : uint32_t {
    RDONLY      = 0x0000,     ///< Read only
    WRONLY      = 0x0001,     ///< Write only
    RDWR        = 0x0002,     ///< Read/write
    CREAT       = 0x0040,     ///< Create if doesn't exist
    EXCL        = 0x0080,     ///< Exclusive open
    TRUNC       = 0x0200,     ///< Truncate
    APPEND      = 0x0400,     ///< Append mode
    NONBLOCK    = 0x0800,     ///< Non-blocking I/O
};

/**
 * @brief Single file descriptor entry
 */
struct FileDescriptor {
    bool is_open;               ///< Is this FD allocated?
    uint32_t flags;             ///< FD flags (FdFlags)
    uint32_t file_flags;        ///< File status flags (FileFlags)
    uint64_t offset;            ///< Current file position
    void* inode;                ///< Pointer to VFS inode
    void* private_data;         ///< Driver-specific data
};

/**
 * @brief File descriptor table (per-process)
 */
constexpr size_t MAX_FDS_PER_PROCESS = 1024;

struct FileDescriptorTable {
    FileDescriptor fds[MAX_FDS_PER_PROCESS];
    uint32_t next_fd;           ///< Hint for next free FD

    /**
     * @brief Allocate a new file descriptor
     * @return FD number, or -EMFILE if table is full
     */
    int allocate_fd();

    /**
     * @brief Get file descriptor entry
     * @param fd File descriptor number
     * @return Pointer to FD entry, or nullptr if invalid
     */
    FileDescriptor* get_fd(int fd);

    /**
     * @brief Close and deallocate file descriptor
     * @param fd File descriptor number
     * @return 0 on success, -EBADF if invalid FD
     */
    int close_fd(int fd);

    /**
     * @brief Duplicate file descriptor
     * @param oldfd Source FD
     * @param newfd Destination FD (or -1 for any)
     * @return New FD number, or negative error
     */
    int dup_fd(int oldfd, int newfd);

    /**
     * @brief Close all FDs marked CLOEXEC
     */
    void close_on_exec();
};

} // namespace xinim::kernel
```

#### Integration with PCB

```cpp
// In src/kernel/pcb.hpp

struct ProcessControlBlock {
    // ... existing fields ...

    // Week 9 Phase 1: File descriptor table
    xinim::kernel::FileDescriptorTable fd_table;

    // ... rest of fields ...
};
```

#### Standard File Descriptors

Every process starts with 3 standard FDs:

| FD | Name | Description | Device |
|----|------|-------------|--------|
| 0 | stdin | Standard input | /dev/console |
| 1 | stdout | Standard output | /dev/console |
| 2 | stderr | Standard error | /dev/console |

**Initialization Code:**
```cpp
void initialize_standard_fds(ProcessControlBlock* pcb) {
    // Open /dev/console for stdin (read-only)
    int fd0 = pcb->fd_table.allocate_fd();
    assert(fd0 == 0);
    FileDescriptor* stdin_fd = pcb->fd_table.get_fd(0);
    stdin_fd->is_open = true;
    stdin_fd->file_flags = (uint32_t)FileFlags::RDONLY;
    stdin_fd->offset = 0;
    stdin_fd->inode = vfs_lookup("/dev/console");

    // Open /dev/console for stdout (write-only)
    int fd1 = pcb->fd_table.allocate_fd();
    assert(fd1 == 1);
    FileDescriptor* stdout_fd = pcb->fd_table.get_fd(1);
    stdout_fd->is_open = true;
    stdout_fd->file_flags = (uint32_t)FileFlags::WRONLY;
    stdout_fd->offset = 0;
    stdout_fd->inode = vfs_lookup("/dev/console");

    // Open /dev/console for stderr (write-only)
    int fd2 = pcb->fd_table.allocate_fd();
    assert(fd2 == 2);
    FileDescriptor* stderr_fd = pcb->fd_table.get_fd(2);
    stderr_fd->is_open = true;
    stderr_fd->file_flags = (uint32_t)FileFlags::WRONLY;
    stderr_fd->offset = 0;
    stderr_fd->inode = vfs_lookup("/dev/console");
}
```

---

### Step 3: VFS-Integrated Syscall Implementations

Now we can implement real syscalls using VFS and FD tables.

#### Files to Modify/Create

1. **src/kernel/syscalls/file_ops.cpp** (~400 LOC) - NEW
2. **src/kernel/syscalls/basic.cpp** - MODIFY sys_write to use FD table

#### sys_open (syscall 2)

```cpp
/**
 * @brief Open file and return file descriptor
 *
 * @param pathname_addr User space pointer to path string
 * @param flags File open flags (O_RDONLY, O_WRONLY, etc.)
 * @param mode File permissions (for creating files)
 * @return FD number on success, negative error code on failure
 */
int64_t sys_open(uint64_t pathname_addr, uint64_t flags, uint64_t mode,
                 uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Copy pathname from user space
    char pathname[PATH_MAX];
    int ret = copy_string_from_user(pathname, pathname_addr, PATH_MAX);
    if (ret < 0) return ret;

    // Validate flags
    uint32_t access_mode = flags & 0x3;  // O_RDONLY, O_WRONLY, O_RDWR
    if (access_mode > 2) return -EINVAL;

    // Resolve path to inode
    void* inode = vfs_lookup(pathname);
    if (!inode) {
        if (flags & (uint32_t)FileFlags::CREAT) {
            // Create new file
            inode = vfs_create(pathname, (uint32_t)mode);
            if (!inode) return -ENOENT;
        } else {
            return -ENOENT;
        }
    }

    // Check permissions
    // TODO Week 9: Implement permission checking

    // Allocate file descriptor
    int fd = current->fd_table.allocate_fd();
    if (fd < 0) return fd;  // -EMFILE (too many open files)

    // Set up FD entry
    FileDescriptor* fd_entry = current->fd_table.get_fd(fd);
    fd_entry->is_open = true;
    fd_entry->flags = 0;
    fd_entry->file_flags = (uint32_t)flags;
    fd_entry->offset = 0;
    fd_entry->inode = inode;
    fd_entry->private_data = nullptr;

    // Truncate if requested
    if (flags & (uint32_t)FileFlags::TRUNC) {
        vfs_truncate(inode, 0);
    }

    return (int64_t)fd;
}
```

#### sys_read (syscall 0)

```cpp
/**
 * @brief Read from file descriptor
 *
 * @param fd File descriptor
 * @param buf_addr User space buffer address
 * @param count Number of bytes to read
 * @return Number of bytes read, or negative error code
 */
int64_t sys_read(uint64_t fd, uint64_t buf_addr, uint64_t count,
                 uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate FD
    if (fd >= MAX_FDS_PER_PROCESS) return -EBADF;
    FileDescriptor* fd_entry = current->fd_table.get_fd((int)fd);
    if (!fd_entry || !fd_entry->is_open) return -EBADF;

    // Check read permission
    uint32_t access_mode = fd_entry->file_flags & 0x3;
    if (access_mode == (uint32_t)FileFlags::WRONLY) return -EBADF;

    // Validate user buffer
    if (!is_user_address(buf_addr, count)) return -EFAULT;

    // Limit read size
    if (count > 4096) count = 4096;

    // Read from VFS into kernel buffer
    char kernel_buf[4096];
    ssize_t bytes_read = vfs_read(fd_entry->inode, kernel_buf, count, fd_entry->offset);
    if (bytes_read < 0) return bytes_read;

    // Copy to user space
    int ret = copy_to_user(buf_addr, kernel_buf, (size_t)bytes_read);
    if (ret < 0) return ret;

    // Update file offset (unless APPEND mode)
    if (!(fd_entry->file_flags & (uint32_t)FileFlags::APPEND)) {
        fd_entry->offset += (uint64_t)bytes_read;
    }

    return bytes_read;
}
```

#### sys_write (syscall 1) - Updated

```cpp
/**
 * @brief Write to file descriptor
 *
 * @param fd File descriptor
 * @param buf_addr User space buffer address
 * @param count Number of bytes to write
 * @return Number of bytes written, or negative error code
 */
int64_t sys_write(uint64_t fd, uint64_t buf_addr, uint64_t count,
                  uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate FD
    if (fd >= MAX_FDS_PER_PROCESS) return -EBADF;
    FileDescriptor* fd_entry = current->fd_table.get_fd((int)fd);
    if (!fd_entry || !fd_entry->is_open) return -EBADF;

    // Check write permission
    uint32_t access_mode = fd_entry->file_flags & 0x3;
    if (access_mode == (uint32_t)FileFlags::RDONLY) return -EBADF;

    // Validate user buffer
    if (!is_user_address(buf_addr, count)) return -EFAULT;

    // Limit write size
    if (count > 4096) count = 4096;

    // Copy from user space to kernel buffer
    char kernel_buf[4096];
    int ret = copy_from_user(kernel_buf, buf_addr, count);
    if (ret < 0) return ret;

    // Write to VFS
    ssize_t bytes_written = vfs_write(fd_entry->inode, kernel_buf, count, fd_entry->offset);
    if (bytes_written < 0) return bytes_written;

    // Update file offset (always append if APPEND flag set)
    if (fd_entry->file_flags & (uint32_t)FileFlags::APPEND) {
        fd_entry->offset = vfs_get_size(fd_entry->inode);
    } else {
        fd_entry->offset += (uint64_t)bytes_written;
    }

    return bytes_written;
}
```

#### sys_close (syscall 3)

```cpp
/**
 * @brief Close file descriptor
 *
 * @param fd File descriptor to close
 * @return 0 on success, negative error code on failure
 */
int64_t sys_close(uint64_t fd, uint64_t, uint64_t,
                  uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate FD
    if (fd >= MAX_FDS_PER_PROCESS) return -EBADF;

    // Close FD (this also validates it)
    return current->fd_table.close_fd((int)fd);
}
```

#### sys_lseek (syscall 8)

```cpp
/**
 * @brief Reposition file offset
 *
 * @param fd File descriptor
 * @param offset New offset (interpretation depends on whence)
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return New file offset, or negative error code
 */
int64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence,
                  uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate FD
    if (fd >= MAX_FDS_PER_PROCESS) return -EBADF;
    FileDescriptor* fd_entry = current->fd_table.get_fd((int)fd);
    if (!fd_entry || !fd_entry->is_open) return -EBADF;

    // Calculate new offset
    int64_t new_offset;
    switch (whence) {
        case 0:  // SEEK_SET
            new_offset = (int64_t)offset;
            break;
        case 1:  // SEEK_CUR
            new_offset = (int64_t)fd_entry->offset + (int64_t)offset;
            break;
        case 2:  // SEEK_END
            new_offset = (int64_t)vfs_get_size(fd_entry->inode) + (int64_t)offset;
            break;
        default:
            return -EINVAL;
    }

    // Validate new offset
    if (new_offset < 0) return -EINVAL;

    // Update offset
    fd_entry->offset = (uint64_t)new_offset;

    return new_offset;
}
```

---

### Step 4: VFS Minimal Implementation

We need minimal VFS functions to support the syscalls. The full VFS exists in `src/vfs/vfs.cpp`, but we need kernel-side wrappers.

#### Files to Create

1. **src/kernel/vfs_interface.hpp** (~100 LOC)
2. **src/kernel/vfs_interface.cpp** (~300 LOC)

#### VFS Interface Functions

```cpp
namespace xinim::kernel {

/**
 * @brief Lookup path and return inode
 *
 * @param pathname Absolute path (e.g., "/dev/console")
 * @return Inode pointer, or nullptr if not found
 */
void* vfs_lookup(const char* pathname);

/**
 * @brief Read from inode
 *
 * @param inode Inode to read from
 * @param buffer Kernel buffer to read into
 * @param count Number of bytes to read
 * @param offset File offset
 * @return Number of bytes read, or negative error
 */
ssize_t vfs_read(void* inode, void* buffer, size_t count, uint64_t offset);

/**
 * @brief Write to inode
 *
 * @param inode Inode to write to
 * @param buffer Kernel buffer to write from
 * @param count Number of bytes to write
 * @param offset File offset
 * @return Number of bytes written, or negative error
 */
ssize_t vfs_write(void* inode, const void* buffer, size_t count, uint64_t offset);

/**
 * @brief Get file size
 *
 * @param inode Inode to query
 * @return File size in bytes
 */
uint64_t vfs_get_size(void* inode);

/**
 * @brief Create new file
 *
 * @param pathname Path to create
 * @param mode File permissions
 * @return Inode pointer, or nullptr on failure
 */
void* vfs_create(const char* pathname, uint32_t mode);

/**
 * @brief Truncate file to specified size
 *
 * @param inode Inode to truncate
 * @param size New size
 * @return 0 on success, negative error on failure
 */
int vfs_truncate(void* inode, uint64_t size);

} // namespace xinim::kernel
```

#### Week 9 Phase 1 Simplification

For Phase 1, we implement a **minimal VFS** that only supports `/dev/console`:

```cpp
// Simple inode structure for Phase 1
struct SimpleInode {
    const char* path;
    bool is_console;
};

static SimpleInode g_console_inode = {
    .path = "/dev/console",
    .is_console = true,
};

void* vfs_lookup(const char* pathname) {
    if (strcmp(pathname, "/dev/console") == 0) {
        return &g_console_inode;
    }
    return nullptr;  // Not found
}

ssize_t vfs_read(void* inode, void* buffer, size_t count, uint64_t offset) {
    SimpleInode* si = (SimpleInode*)inode;
    if (!si->is_console) return -EIO;

    // Read from serial console (if available)
    // For now, return 0 (EOF) since serial input is complex
    return 0;
}

ssize_t vfs_write(void* inode, const void* buffer, size_t count, uint64_t offset) {
    SimpleInode* si = (SimpleInode*)inode;
    if (!si->is_console) return -EIO;

    // Write to serial console
    extern xinim::early::Serial16550 early_serial;
    const char* buf = (const char*)buffer;
    for (size_t i = 0; i < count; i++) {
        early_serial.write_char(buf[i]);
    }

    return (ssize_t)count;
}
```

---

## Testing Strategy

### Unit Tests

1. **User pointer validation:**
   ```cpp
   assert(is_user_address(0x1000, 100) == true);
   assert(is_user_address(0x0, 100) == false);  // Null page
   assert(is_user_address(0xFFFF800000000000, 100) == false);  // Kernel space
   ```

2. **FD allocation:**
   ```cpp
   int fd1 = allocate_fd();  // Should be 3 (after stdin/stdout/stderr)
   int fd2 = allocate_fd();  // Should be 4
   close_fd(fd1);
   int fd3 = allocate_fd();  // Should reuse 3
   ```

3. **Copy functions:**
   ```cpp
   char kernel_buf[100];
   int ret = copy_from_user(kernel_buf, user_addr, 50);
   assert(ret == 0);
   assert(memcmp(kernel_buf, expected, 50) == 0);
   ```

### Integration Tests

1. **Open/write/close from VFS server:**
   ```c
   int fd = open("/dev/console", O_WRONLY);
   assert(fd >= 0);

   ssize_t n = write(fd, "Hello VFS!\n", 11);
   assert(n == 11);

   int ret = close(fd);
   assert(ret == 0);
   ```

2. **Multiple FDs:**
   ```c
   int fd1 = open("/dev/console", O_RDWR);
   int fd2 = open("/dev/console", O_RDWR);
   assert(fd1 != fd2);

   write(fd1, "FD1\n", 4);
   write(fd2, "FD2\n", 4);

   close(fd1);
   close(fd2);
   ```

### Expected Serial Output

```
[SYSCALL] sys_open("/dev/console", O_WRONLY) = 3
[VFS] Lookup: /dev/console → inode 0xffffffff80abcdef
[FD] Allocated FD 3 for PID 2
Hello VFS!
[SYSCALL] sys_close(3) = 0
[FD] Closed FD 3 for PID 2
```

---

## File Checklist

### New Files to Create (8)

- [ ] `src/kernel/uaccess.hpp` - User pointer validation declarations
- [ ] `src/kernel/uaccess.cpp` - User pointer validation implementation
- [ ] `src/kernel/fd_table.hpp` - File descriptor table declarations
- [ ] `src/kernel/fd_table.cpp` - File descriptor table implementation
- [ ] `src/kernel/vfs_interface.hpp` - VFS interface declarations
- [ ] `src/kernel/vfs_interface.cpp` - VFS minimal implementation
- [ ] `src/kernel/syscalls/file_ops.cpp` - File operation syscalls
- [ ] `docs/WEEK9_PHASE1_COMPLETE.md` - Completion documentation

### Files to Modify (4)

- [ ] `src/kernel/pcb.hpp` - Add `FileDescriptorTable fd_table;`
- [ ] `src/kernel/syscall_table.cpp` - Update dispatch table with new syscalls
- [ ] `src/kernel/server_spawn.cpp` - Initialize standard FDs for new processes
- [ ] `xmake.lua` - Add new files to build

---

## Build Integration

```lua
-- Week 9 Phase 1: VFS integration
add_files("src/kernel/uaccess.cpp")
add_files("src/kernel/fd_table.cpp")
add_files("src/kernel/vfs_interface.cpp")
add_files("src/kernel/syscalls/file_ops.cpp")
```

---

## Security Considerations

### Week 9 Phase 1

1. **User pointer validation** prevents kernel crashes from malicious pointers
2. **FD bounds checking** prevents out-of-bounds access
3. **Permission checking** (basic: read/write flags)
4. **Path traversal prevention** (via VFS path resolution)

### Future Enhancements

1. **Page table validation** - Check if user pages are actually mapped
2. **Capability-based security** - Check if process has permission
3. **Resource limits** - Limit max open FDs per process (RLIMIT_NOFILE)
4. **Audit logging** - Log all syscalls for security analysis

---

## Performance Targets

| Operation | Target | Notes |
|-----------|--------|-------|
| `copy_from_user()` | < 100 ns | Memcpy + validation |
| `open()` syscall | < 5 μs | Path lookup + FD allocation |
| `read()` syscall | < 2 μs | FD lookup + VFS read + copy_to_user |
| `write()` syscall | < 2 μs | FD lookup + copy_from_user + VFS write |
| `close()` syscall | < 1 μs | FD deallocation |

---

## Implementation Order

1. **User pointer validation** (foundation for all syscalls)
2. **File descriptor tables** (state management)
3. **VFS interface** (minimal /dev/console support)
4. **sys_open** (file opening)
5. **sys_read** (reading data)
6. **sys_write** (update to use FD table)
7. **sys_close** (cleanup)
8. **sys_lseek** (file positioning)
9. **Integration testing**
10. **Documentation**

---

## Success Metrics

- [ ] All user pointers validated before dereferencing
- [ ] No kernel panics from malicious user input
- [ ] FD table properly manages up to 1024 FDs per process
- [ ] Standard FDs (0, 1, 2) initialized for all processes
- [ ] VFS server can open/read/write/close /dev/console
- [ ] File operations work from Ring 3
- [ ] All syscalls return proper POSIX error codes
- [ ] Performance targets met

---

## Timeline Estimate

| Task | Estimated Time | Complexity |
|------|---------------|------------|
| User pointer validation | 1-2 hours | Medium |
| File descriptor tables | 2-3 hours | Medium |
| VFS interface | 1-2 hours | Low |
| File operation syscalls | 3-4 hours | High |
| Testing | 2-3 hours | Medium |
| Documentation | 1-2 hours | Low |
| **Total** | **10-16 hours** | **Week 9 Phase 1** |

---

## Risk Analysis

| Risk | Impact | Mitigation |
|------|--------|------------|
| User pointer validation incomplete | High | Extensive testing with malicious inputs |
| VFS complexity underestimated | Medium | Start with minimal /dev/console only |
| Performance degradation | Low | Profile and optimize copy functions |
| FD leaks | Medium | Careful testing of close() and error paths |

---

## Next Phases (Week 9+)

### Week 9 Phase 2: Process Management
- `fork()` - Create child process
- `exec()` - Execute new program
- `wait4()` - Wait for child process
- `getppid()` - Get parent PID

### Week 9 Phase 3: Advanced File Operations
- `dup()` / `dup2()` - Duplicate FDs
- `pipe()` - Create pipe
- `fcntl()` - File control operations
- `ioctl()` - Device control

### Week 10: Full VFS
- Real file systems (ext2, tmpfs)
- Directory operations (mkdir, rmdir, readdir)
- File metadata (stat, chmod, chown)
- Hard/soft links

---

## Conclusion

Week 9 Phase 1 establishes the **foundation for real file operations** in XINIM. By implementing user pointer validation, file descriptor tables, and VFS integration, we transform the syscall infrastructure from a proof-of-concept into a production-quality system capable of supporting POSIX applications.

**Phase 1 Deliverables:**
- Secure user pointer validation
- Per-process file descriptor management
- VFS-integrated syscalls (open, read, write, close, lseek)
- Full testing and documentation

**After Phase 1, XINIM will be able to:**
- ✅ Open files from Ring 3
- ✅ Read/write data through VFS
- ✅ Manage file descriptors per-process
- ✅ Prevent kernel crashes from malicious user input

---

**Status:** 📋 **READY TO IMPLEMENT**

**Author:** XINIM Development Team
**Date:** November 18, 2025
**Version:** 1.0
