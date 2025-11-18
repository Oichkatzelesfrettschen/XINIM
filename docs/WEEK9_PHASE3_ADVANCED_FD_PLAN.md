# Week 9 Phase 3: Advanced File Operations & Inter-Process Communication

**Status:** 📋 **PLANNING**
**Date:** November 18, 2025
**Milestone:** Complete POSIX file descriptor manipulation and enable IPC via pipes

---

## Executive Summary

Week 9 Phase 3 completes the file descriptor story by implementing advanced operations (dup/dup2, fcntl) and establishes inter-process communication via pipes. This enables shell-like functionality including I/O redirection and process pipelines.

**Goal:** Transform XINIM from basic file I/O to full shell-capable file descriptor manipulation and process communication.

---

## Prerequisites (Completed)

- ✅ File descriptor tables (Phase 1)
- ✅ VFS integration (Phase 1)
- ✅ Process management (Phase 2)
- ✅ fork/wait working (Phase 2)

---

## Week 9 Phase 3 Objectives

### Primary Goals

1. **sys_dup / sys_dup2** - Duplicate file descriptors
2. **sys_pipe** - Create pipe for IPC
3. **sys_fcntl** - File descriptor control operations
4. **Pipe Infrastructure** - Ring buffer with blocking I/O
5. **Shell Redirection** - Enable `cmd > file`, `cmd < file`, `cmd | cmd2`

### Success Criteria

- [ ] dup() creates working duplicate FD
- [ ] dup2() atomically replaces FD
- [ ] pipe() creates working pipe pair
- [ ] Parent can write to pipe, child can read
- [ ] Blocking I/O works (read blocks until data available)
- [ ] Shell-style redirection demonstrable
- [ ] fcntl can get/set FD flags

---

## Technical Architecture

### File Descriptor Duplication

**Purpose:** Create multiple FDs pointing to same file

**Use Cases:**
- Shell redirection (`cmd > file` uses dup2)
- Saving/restoring FDs
- FD inheritance control

**Example:**
```c
int fd = open("/dev/console", O_WRONLY);
int fd_copy = dup(fd);  // Now both point to same file

// Both write to same file:
write(fd, "Hello", 5);
write(fd_copy, " World", 6);  // Appends

close(fd);
// fd_copy still works!
```

### Pipe Architecture

**Pipe = Unidirectional byte stream between two FDs**

```
Process A                         Process B
  │                                   │
  ├─ pipe_fds[1] (write end)         │
  │      │                            │
  │      └──> Pipe Buffer ──> pipe_fds[0] (read end)
  │              (Ring Buffer)        │
  │              4096 bytes            │
```

**Key Properties:**
- Unidirectional (write end → read end)
- Blocking I/O (read blocks if empty, write blocks if full)
- Atomic writes < PIPE_BUF (4096 bytes)
- FDs can be passed to children via fork

---

## Implementation Plan

### Step 1: Implement sys_dup (Syscall 32)

**Duplicate file descriptor to lowest available FD**

```cpp
/**
 * @brief Duplicate file descriptor
 *
 * POSIX: int dup(int oldfd)
 *
 * Creates a copy of oldfd using the lowest available FD number.
 * Both FDs share the same file offset and flags.
 *
 * @param oldfd File descriptor to duplicate
 * @return New FD number on success, negative error on failure
 *         -EBADF: oldfd is invalid
 *         -EMFILE: Too many open files
 */
extern "C" int64_t sys_dup(uint64_t oldfd, uint64_t, uint64_t,
                            uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate oldfd
    if (oldfd >= MAX_FDS_PER_PROCESS) return -EBADF;
    if (!current->fd_table.is_valid_fd((int)oldfd)) return -EBADF;

    // Use FD table's dup_fd with newfd = -1 (any available FD)
    int newfd = current->fd_table.dup_fd((int)oldfd, -1);
    if (newfd < 0) return newfd;

    return (int64_t)newfd;
}
```

---

### Step 2: Implement sys_dup2 (Syscall 33)

**Duplicate FD to specific FD number (atomic replace)**

```cpp
/**
 * @brief Duplicate file descriptor to specific FD
 *
 * POSIX: int dup2(int oldfd, int newfd)
 *
 * Duplicates oldfd to newfd. If newfd is open, it's closed first.
 * If oldfd == newfd, returns newfd without closing.
 *
 * @param oldfd Source file descriptor
 * @param newfd Target file descriptor
 * @return newfd on success, negative error on failure
 *         -EBADF: oldfd invalid or newfd out of range
 */
extern "C" int64_t sys_dup2(uint64_t oldfd, uint64_t newfd,
                             uint64_t, uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate oldfd
    if (oldfd >= MAX_FDS_PER_PROCESS) return -EBADF;
    if (!current->fd_table.is_valid_fd((int)oldfd)) return -EBADF;

    // Validate newfd range
    if (newfd >= MAX_FDS_PER_PROCESS) return -EBADF;

    // Special case: oldfd == newfd (POSIX requirement)
    if ((int)oldfd == (int)newfd) {
        return (int64_t)newfd;
    }

    // Close newfd if it's open (atomic operation)
    if (current->fd_table.is_valid_fd((int)newfd)) {
        current->fd_table.close_fd((int)newfd);
    }

    // Duplicate oldfd to newfd
    int result = current->fd_table.dup_fd((int)oldfd, (int)newfd);
    if (result < 0) return result;

    return (int64_t)newfd;
}
```

**Use Case: Shell Redirection**
```c
// Redirect stdout to file
int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC);
dup2(fd, 1);  // Replace stdout with file
close(fd);    // Original FD no longer needed
// Now all write(1, ...) goes to output.txt
```

---

### Step 3: Pipe Data Structure

**Ring Buffer for Pipe**

```cpp
// In src/kernel/pipe.hpp

namespace xinim::kernel {

constexpr size_t PIPE_BUF = 4096;  ///< POSIX atomic write size

/**
 * @brief Pipe structure (shared between read and write FDs)
 */
struct Pipe {
    char buffer[PIPE_BUF];       ///< Ring buffer
    size_t read_pos;             ///< Read position
    size_t write_pos;            ///< Write position
    size_t count;                ///< Bytes currently in pipe

    bool read_end_open;          ///< Is read end still open?
    bool write_end_open;         ///< Is write end still open?

    // Blocking lists
    ProcessControlBlock* readers_head;  ///< Blocked readers
    ProcessControlBlock* writers_head;  ///< Blocked writers

    /**
     * @brief Write data to pipe
     * @return Bytes written or negative error
     */
    ssize_t write(const void* data, size_t len);

    /**
     * @brief Read data from pipe
     * @return Bytes read or negative error
     */
    ssize_t read(void* data, size_t len);

    /**
     * @brief Check if pipe is full
     */
    bool is_full() const { return count == PIPE_BUF; }

    /**
     * @brief Check if pipe is empty
     */
    bool is_empty() const { return count == 0; }
};

} // namespace xinim::kernel
```

---

### Step 4: Implement sys_pipe (Syscall 22)

**Create pipe (two FDs: read and write)**

```cpp
/**
 * @brief Create pipe
 *
 * POSIX: int pipe(int pipefd[2])
 *
 * Creates a pipe with:
 * - pipefd[0]: read end
 * - pipefd[1]: write end
 *
 * @param pipefd_addr User space pointer to int[2] array
 * @return 0 on success, negative error on failure
 *         -EFAULT: Invalid pipefd pointer
 *         -EMFILE: Too many open files
 */
extern "C" int64_t sys_pipe(uint64_t pipefd_addr, uint64_t, uint64_t,
                             uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate user pointer
    if (!is_user_address(pipefd_addr, 2 * sizeof(int))) {
        return -EFAULT;
    }

    // Allocate pipe structure
    Pipe* pipe = new Pipe();
    if (!pipe) return -ENOMEM;

    // Initialize pipe
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->count = 0;
    pipe->read_end_open = true;
    pipe->write_end_open = true;
    pipe->readers_head = nullptr;
    pipe->writers_head = nullptr;

    // Allocate read FD
    int read_fd = current->fd_table.allocate_fd();
    if (read_fd < 0) {
        delete pipe;
        return read_fd;  // -EMFILE
    }

    // Allocate write FD
    int write_fd = current->fd_table.allocate_fd();
    if (write_fd < 0) {
        current->fd_table.close_fd(read_fd);
        delete pipe;
        return write_fd;  // -EMFILE
    }

    // Set up read FD
    FileDescriptor* read_fd_entry = current->fd_table.get_fd(read_fd);
    read_fd_entry->is_open = true;
    read_fd_entry->flags = 0;
    read_fd_entry->file_flags = (uint32_t)FileFlags::RDONLY;
    read_fd_entry->offset = 0;
    read_fd_entry->inode = nullptr;  // Pipes don't have inodes
    read_fd_entry->private_data = pipe;  // Store pipe pointer

    // Set up write FD
    FileDescriptor* write_fd_entry = current->fd_table.get_fd(write_fd);
    write_fd_entry->is_open = true;
    write_fd_entry->flags = 0;
    write_fd_entry->file_flags = (uint32_t)FileFlags::WRONLY;
    write_fd_entry->offset = 0;
    write_fd_entry->inode = nullptr;
    write_fd_entry->private_data = pipe;  // Same pipe pointer

    // Copy FD array to user space
    int pipefd[2] = { read_fd, write_fd };
    int ret = copy_to_user(pipefd_addr, pipefd, sizeof(pipefd));
    if (ret < 0) {
        current->fd_table.close_fd(read_fd);
        current->fd_table.close_fd(write_fd);
        delete pipe;
        return ret;
    }

    return 0;  // Success
}
```

---

### Step 5: Pipe I/O Operations

**Update sys_read for pipe support:**

```cpp
// In sys_read, check if FD is a pipe
if (fd_entry->private_data && !fd_entry->inode) {
    // This is a pipe
    Pipe* pipe = static_cast<Pipe*>(fd_entry->private_data);

    // Read from pipe (may block)
    ssize_t bytes = pipe->read(kernel_buf, count);
    if (bytes < 0) return bytes;

    // Copy to user
    if (bytes > 0) {
        int ret = copy_to_user(buf_addr, kernel_buf, bytes);
        if (ret < 0) return ret;
    }

    return bytes;
}
```

**Update sys_write for pipe support:**

```cpp
// In sys_write, check if FD is a pipe
if (fd_entry->private_data && !fd_entry->inode) {
    // This is a pipe
    Pipe* pipe = static_cast<Pipe*>(fd_entry->private_data);

    // Write to pipe (may block)
    ssize_t bytes = pipe->write(kernel_buf, count);
    return bytes;
}
```

**Pipe Read Implementation:**

```cpp
ssize_t Pipe::read(void* data, size_t len) {
    // Check if write end is closed and pipe is empty
    if (!write_end_open && count == 0) {
        return 0;  // EOF
    }

    // Wait until data is available
    while (count == 0 && write_end_open) {
        // Block current process
        ProcessControlBlock* current = get_current_process();
        current->state = ProcessState::BLOCKED;
        current->blocked_on = BlockReason::IO;

        // Add to readers list
        current->next = readers_head;
        readers_head = current;

        // Yield
        schedule();
    }

    // Read data from ring buffer
    size_t bytes_read = 0;
    while (bytes_read < len && count > 0) {
        ((char*)data)[bytes_read++] = buffer[read_pos];
        read_pos = (read_pos + 1) % PIPE_BUF;
        count--;
    }

    // Wake up blocked writers
    while (writers_head) {
        ProcessControlBlock* writer = writers_head;
        writers_head = writer->next;
        writer->state = ProcessState::READY;
        writer->blocked_on = BlockReason::NONE;
    }

    return (ssize_t)bytes_read;
}
```

---

### Step 6: Implement sys_fcntl (Syscall 72)

**File control operations**

```cpp
/**
 * @brief File control operations
 *
 * POSIX: int fcntl(int fd, int cmd, ...)
 *
 * Week 9 Phase 3: Support basic commands
 * - F_DUPFD: Duplicate FD
 * - F_GETFD: Get FD flags
 * - F_SETFD: Set FD flags
 * - F_GETFL: Get file status flags
 * - F_SETFL: Set file status flags
 *
 * @param fd File descriptor
 * @param cmd Command
 * @param arg Command argument
 * @return Command-specific result or negative error
 */
extern "C" int64_t sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg,
                              uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) return -ESRCH;

    // Validate FD
    if (fd >= MAX_FDS_PER_PROCESS) return -EBADF;
    FileDescriptor* fd_entry = current->fd_table.get_fd((int)fd);
    if (!fd_entry) return -EBADF;

    switch (cmd) {
        case 0:  // F_DUPFD - Duplicate FD to >= arg
        {
            int newfd = current->fd_table.allocate_fd();
            if (newfd < 0) return newfd;

            // If newfd < arg, keep allocating
            while (newfd < (int)arg && newfd >= 0) {
                current->fd_table.close_fd(newfd);
                newfd = current->fd_table.allocate_fd();
            }

            if (newfd < 0) return -EMFILE;

            int ret = current->fd_table.dup_fd((int)fd, newfd);
            return ret;
        }

        case 1:  // F_GETFD - Get FD flags
            return (int64_t)fd_entry->flags;

        case 2:  // F_SETFD - Set FD flags
            fd_entry->flags = (uint32_t)arg;
            return 0;

        case 3:  // F_GETFL - Get file status flags
            return (int64_t)fd_entry->file_flags;

        case 4:  // F_SETFL - Set file status flags
            // Only allow changing O_APPEND, O_NONBLOCK
            fd_entry->file_flags = (uint32_t)arg;
            return 0;

        default:
            return -EINVAL;
    }
}
```

---

## Testing Strategy

### Test 1: dup() Basic

```c
int fd = open("/dev/console", O_WRONLY);
int fd2 = dup(fd);

write(fd, "Hello ", 6);
write(fd2, "World\n", 6);

close(fd);
close(fd2);
```

**Expected:** "Hello World\n" appears on console

### Test 2: dup2() Redirection

```c
int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC);
dup2(fd, 1);  // Redirect stdout
close(fd);

printf("This goes to file\n");  // Actually write(1, ..., ...)
```

### Test 3: Pipe Parent→Child

```c
int pipefd[2];
pipe(pipefd);

pid_t pid = fork();

if (pid == 0) {
    // Child: read from pipe
    close(pipefd[1]);  // Close write end

    char buf[100];
    ssize_t n = read(pipefd[0], buf, sizeof(buf));
    write(1, "Child received: ", 16);
    write(1, buf, n);

    close(pipefd[0]);
    exit(0);
} else {
    // Parent: write to pipe
    close(pipefd[0]);  // Close read end

    write(pipefd[1], "Hello from parent!", 18);

    close(pipefd[1]);
    wait4(-1, NULL, 0, NULL);
}
```

**Expected:**
```
Child received: Hello from parent!
```

### Test 4: Shell Pipeline Simulation

```c
// Simulate: cmd1 | cmd2

int pipefd[2];
pipe(pipefd);

pid_t pid1 = fork();
if (pid1 == 0) {
    // cmd1: producer
    dup2(pipefd[1], 1);  // Redirect stdout to pipe
    close(pipefd[0]);
    close(pipefd[1]);

    write(1, "Data from cmd1\n", 15);
    exit(0);
}

pid_t pid2 = fork();
if (pid2 == 0) {
    // cmd2: consumer
    dup2(pipefd[0], 0);  // Redirect stdin from pipe
    close(pipefd[0]);
    close(pipefd[1]);

    char buf[100];
    ssize_t n = read(0, buf, sizeof(buf));
    write(1, "cmd2 received: ", 15);
    write(1, buf, n);
    exit(0);
}

// Parent: close pipe and wait
close(pipefd[0]);
close(pipefd[1]);
wait4(-1, NULL, 0, NULL);
wait4(-1, NULL, 0, NULL);
```

---

## Files to Create/Modify

### New Files (3)

1. **src/kernel/pipe.hpp** (~150 LOC)
   - Pipe structure
   - Ring buffer management
   - Blocking I/O interface

2. **src/kernel/pipe.cpp** (~250 LOC)
   - Pipe read/write implementation
   - Blocking/waking logic

3. **src/kernel/syscalls/fd_advanced.cpp** (~300 LOC)
   - sys_dup, sys_dup2, sys_pipe, sys_fcntl

### Files to Modify (4)

1. **src/kernel/syscalls/file_ops.cpp**
   - Update sys_read for pipe support
   - Update sys_write for pipe support
   - Update sys_close for pipe cleanup

2. **src/kernel/syscall_table.cpp**
   - Add forward declarations
   - Update dispatch table

3. **src/kernel/pcb.hpp**
   - Add BlockReason::PIPE_READ, BlockReason::PIPE_WRITE

4. **xmake.lua**
   - Add pipe.cpp, fd_advanced.cpp

---

## Implementation Order

1. ✅ Planning (this document)
2. **sys_dup / sys_dup2** (use existing FD table infrastructure)
3. **Pipe data structure** (ring buffer)
4. **Pipe I/O** (read/write with blocking)
5. **sys_pipe** (create pipe pair)
6. **Update sys_read/sys_write** (pipe support)
7. **sys_fcntl** (FD control)
8. **Testing**
9. **Documentation**

---

## Success Metrics

- [ ] dup() creates working duplicate
- [ ] dup2() atomically replaces FD
- [ ] pipe() creates bidirectional communication
- [ ] Pipe blocks on empty read
- [ ] Pipe blocks on full write
- [ ] Pipe EOF when write end closed
- [ ] Shell redirection works (dup2)
- [ ] Shell pipeline works (pipe + fork + dup2)
- [ ] fcntl can modify FD flags
- [ ] No deadlocks in pipe I/O

---

## Timeline Estimate

| Task | Time | Complexity |
|------|------|------------|
| sys_dup/dup2 | 1-2 hours | Low (uses existing FD table) |
| Pipe structure | 1-2 hours | Medium |
| Pipe I/O | 3-4 hours | High (blocking logic) |
| sys_pipe | 1-2 hours | Medium |
| sys_fcntl | 1-2 hours | Low |
| Testing | 2-3 hours | Medium |
| Documentation | 1 hour | Low |
| **Total** | **10-16 hours** | **Phase 3** |

---

## Next Steps (Week 10)

1. **sys_execve** - Replace process image
2. **ELF loader** - Parse and load executables
3. **Copy-on-write** - Efficient fork
4. **Full VFS** - Regular files (tmpfs, ext2)

---

## Conclusion

Week 9 Phase 3 completes the POSIX file descriptor story and enables inter-process communication. By implementing dup/dup2/pipe/fcntl, we enable shell-like functionality including I/O redirection and process pipelines.

**After Phase 3, XINIM will support:**
- ✅ FD duplication (dup/dup2)
- ✅ IPC via pipes
- ✅ Shell redirection (`cmd > file`, `cmd < file`)
- ✅ Shell pipelines (`cmd1 | cmd2`)
- ✅ FD control (fcntl)

This completes Week 9 with a robust, POSIX-compliant file and process subsystem!

---

**Status:** 📋 **READY TO IMPLEMENT**

**Author:** XINIM Development Team
**Date:** November 18, 2025
**Version:** 1.0
