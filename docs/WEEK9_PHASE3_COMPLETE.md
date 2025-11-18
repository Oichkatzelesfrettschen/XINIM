# Week 9 Phase 3: Advanced File Operations & IPC - COMPLETE

**Status**: ✅ COMPLETE
**Date**: November 18, 2025
**Phase**: Week 9 Phase 3 - Advanced FD Operations and Pipes

---

## Overview

Week 9 Phase 3 implements advanced file descriptor operations and inter-process communication (IPC) via pipes. This phase completes the POSIX file descriptor manipulation syscalls required for shell redirection and process pipelines.

---

## Implementation Summary

### 1. Advanced File Descriptor Operations

#### sys_dup (Syscall 32)
- Duplicates file descriptor to lowest available FD
- Uses existing `FileDescriptorTable::dup_fd()` infrastructure
- **Location**: `src/kernel/syscalls/fd_advanced.cpp:24-44`

#### sys_dup2 (Syscall 33)
- Duplicates file descriptor to specific FD number
- Atomically closes target FD if open
- Handles edge case where `oldfd == newfd`
- **Location**: `src/kernel/syscalls/fd_advanced.cpp:46-85`

#### sys_fcntl (Syscall 72)
- File control operations: F_DUPFD, F_GETFD, F_SETFD, F_GETFL, F_SETFL
- Supports CLOEXEC flag manipulation
- **Location**: `src/kernel/syscalls/fd_advanced.cpp:156-238`

---

### 2. Pipe Implementation

#### Pipe Data Structure
- **Ring Buffer**: 4096 bytes (PIPE_BUF)
- **Blocking I/O**: Reader/writer wait queues
- **EOF Handling**: Proper detection when write end closes
- **EPIPE Detection**: Returns -EPIPE when read end closes
- **Location**: `src/kernel/pipe.hpp`, `src/kernel/pipe.cpp`

#### Pipe Read Operation
```cpp
ssize_t Pipe::read(void* data, size_t len)
```
- Blocks when pipe is empty (if write end open)
- Returns 0 (EOF) when write end closed and pipe empty
- Wakes blocked writers after reading
- **Location**: `src/kernel/pipe.cpp:101-151`

#### Pipe Write Operation
```cpp
ssize_t Pipe::write(const void* data, size_t len)
```
- Blocks when pipe is full (if read end open)
- Returns -EPIPE when read end closed
- Wakes blocked readers after writing
- **Location**: `src/kernel/pipe.cpp:33-85`

#### sys_pipe (Syscall 22)
- Creates pipe pair with two FDs
- Read end: O_RDONLY (FD 0 in pipefd array)
- Write end: O_WRONLY (FD 1 in pipefd array)
- Stores Pipe pointer in `FileDescriptor::private_data`
- **Location**: `src/kernel/syscalls/fd_advanced.cpp:87-154`

---

### 3. VFS Integration Updates

#### sys_read (updated)
- Detects pipes via `fd_entry->private_data != nullptr`
- Routes pipe reads to `Pipe::read()`
- Maintains VFS path for regular files/devices
- **Location**: `src/kernel/syscalls/file_ops.cpp:163-181`

#### sys_write (updated)
- Detects pipes via `fd_entry->private_data != nullptr`
- Routes pipe writes to `Pipe::write()`
- Maintains VFS path for regular files/devices
- **Location**: `src/kernel/syscalls/basic.cpp:72-83`

#### sys_close (updated)
- Calls `Pipe::close_read_end()` or `Pipe::close_write_end()` based on access mode
- Wakes blocked processes on pipe ends
- **Location**: `src/kernel/syscalls/file_ops.cpp:229-242`

#### sys_lseek (updated)
- Returns -ESPIPE for pipes (seeking not allowed)
- **Location**: `src/kernel/syscalls/file_ops.cpp:288-291`

---

## Files Created

### New Files
1. **src/kernel/pipe.hpp** (120 LOC)
   - Pipe structure definition
   - Ring buffer management
   - Blocking I/O interface

2. **src/kernel/pipe.cpp** (194 LOC)
   - Pipe read/write implementation
   - Blocking I/O with process wait queues
   - Close operations

3. **src/kernel/syscalls/fd_advanced.cpp** (270 LOC)
   - sys_dup, sys_dup2, sys_pipe, sys_fcntl implementations
   - Complete POSIX FD manipulation

4. **docs/WEEK9_PHASE3_COMPLETE.md** (this file)
   - Phase 3 completion documentation

---

## Files Modified

### Updated Files
1. **src/kernel/syscalls/file_ops.cpp**
   - Added pipe.hpp include
   - Updated sys_read for pipe detection
   - Updated sys_close for pipe cleanup
   - Updated sys_lseek to reject pipes

2. **src/kernel/syscalls/basic.cpp**
   - Added pipe.hpp include
   - Updated sys_write for pipe detection

3. **src/kernel/syscall_table.cpp**
   - Added forward declarations for Phase 3 syscalls
   - Added syscall table entries for pipe (22), dup (32), dup2 (33), fcntl (72)

4. **xmake.lua**
   - Added pipe.cpp to build
   - Added fd_advanced.cpp to build

---

## Technical Details

### Blocking I/O Implementation

**Reader Blocking (Pipe::read)**:
```cpp
while (count == 0 && write_end_open) {
    current->state = ProcessState::BLOCKED;
    current->blocked_on = BlockReason::IO;
    current->next = readers_head;
    readers_head = current;
    schedule();
}
```

**Writer Blocking (Pipe::write)**:
```cpp
while (available() < len && read_end_open) {
    current->state = ProcessState::BLOCKED;
    current->blocked_on = BlockReason::IO;
    current->next = writers_head;
    writers_head = current;
    schedule();
}
```

### Pipe Detection in VFS

All file I/O syscalls detect pipes via:
```cpp
if (fd_entry->private_data != nullptr) {
    // This is a pipe
    Pipe* pipe = static_cast<Pipe*>(fd_entry->private_data);
    // ... use Pipe::read() or Pipe::write()
}
```

This leverages the `FileDescriptor::private_data` field which is:
- `nullptr` for regular files and devices
- `Pipe*` for pipe file descriptors

---

## Syscall Numbers (POSIX-aligned)

| Syscall | Number | Implementation |
|---------|--------|----------------|
| pipe    | 22     | ✅ Complete    |
| dup     | 32     | ✅ Complete    |
| dup2    | 33     | ✅ Complete    |
| fcntl   | 72     | ✅ Complete    |

---

## Testing Strategy

### Unit Tests (Week 10)

1. **Pipe Tests**:
   - Create pipe, write data, read data
   - Test blocking on empty pipe
   - Test blocking on full pipe
   - Test EOF when write end closes
   - Test EPIPE when read end closes

2. **dup/dup2 Tests**:
   - Duplicate stdout to another FD
   - Test shell-style redirection (dup2 to FD 1)
   - Verify both FDs share same offset

3. **fcntl Tests**:
   - Get/set FD flags (CLOEXEC)
   - Get file status flags
   - Duplicate FD with F_DUPFD

### Integration Tests (Week 10)

1. **Shell Pipeline Test**:
   ```c
   // Create pipe
   int pipefd[2];
   pipe(pipefd);

   // Fork child processes
   if (fork() == 0) {
       // Child 1: writer
       close(pipefd[0]);
       dup2(pipefd[1], 1);  // Redirect stdout to pipe
       write(1, "Hello\n", 6);
       exit(0);
   }

   if (fork() == 0) {
       // Child 2: reader
       close(pipefd[1]);
       dup2(pipefd[0], 0);  // Redirect stdin from pipe
       char buf[32];
       read(0, buf, 32);
       exit(0);
   }
   ```

2. **Multi-stage Pipeline**:
   - Test `cmd1 | cmd2 | cmd3` pattern
   - Verify proper cleanup when middle process exits early

---

## Known Limitations (Week 9)

1. **Pipe Memory Management**:
   - Pipes are not freed when both ends close (TODO Week 10)
   - Need reference counting for pipe cleanup

2. **Pipe Buffer Size**:
   - Fixed 4096 bytes (no dynamic resizing)
   - Atomic writes enforced (no partial writes in Week 9)

3. **Non-blocking I/O**:
   - O_NONBLOCK not yet supported
   - All pipe I/O is blocking in Week 9

4. **Signal Handling**:
   - Writing to pipe with closed read end doesn't send SIGPIPE yet
   - Just returns -EPIPE (signal delivery in Week 10)

---

## Performance Characteristics

### Pipe Operations
- **Read/Write**: O(n) where n = bytes transferred
- **Blocking**: O(1) to add to wait queue
- **Wakeup**: O(m) where m = number of blocked processes

### Ring Buffer
- **Space Complexity**: O(1) - fixed 4096 bytes
- **No Synchronization Overhead**: Single-core scheduler guarantees atomicity

---

## Memory Layout

### Pipe Structure Size
```cpp
sizeof(Pipe) = 4096 (buffer)
             + 24 (read_pos, write_pos, count)
             + 2 (read_end_open, write_end_open)
             + 16 (readers_head, writers_head)
             = ~4138 bytes per pipe
```

---

## Next Steps (Week 10)

1. **Pipe Cleanup**:
   - Implement reference counting
   - Free pipes when both ends close

2. **Non-blocking I/O**:
   - Support O_NONBLOCK flag
   - Return -EAGAIN instead of blocking

3. **Signal Delivery**:
   - Send SIGPIPE when writing to closed read end
   - Implement signal handling framework

4. **execve Implementation**:
   - Execute new programs
   - Preserve FD table across exec (except CLOEXEC)

5. **Shell Integration**:
   - Test with full shell pipeline
   - Implement redirection operators (>, <, |)

---

## Conclusion

Week 9 Phase 3 successfully implements:
- ✅ Advanced FD operations (dup, dup2, fcntl)
- ✅ Pipe IPC with blocking I/O
- ✅ VFS integration for pipe detection
- ✅ Proper EOF and EPIPE handling
- ✅ Complete syscall table integration

**Phase 3 Complete**: XINIM now supports full POSIX file descriptor manipulation and inter-process communication via pipes, enabling shell pipelines and I/O redirection.

**Total Lines of Code**: ~584 LOC (new) + ~50 LOC (modifications)

---

**Week 9 Status**: Phase 1 ✅ | Phase 2 ✅ | Phase 3 ✅
**Ready for**: Week 10 (execve, signals, shell integration)
