# XINIM System Call Implementation Guide
**Version:** 1.0
**Date:** 2025-11-17
**Target:** Weeks 5-8 (Phase 2: Kernel Syscalls)

---

## 1. Overview

This guide provides detailed instructions for implementing the 300 POSIX system calls required for full SUSv4 POSIX.1-2017 compliance in the XINIM microkernel.

### 1.1 Current Status

- **Implemented:** ~60 syscalls
- **Required:** 300 syscalls
- **Gap:** 240 syscalls to implement

### 1.2 Implementation Schedule

```
Week 5: Core syscalls (60) - Process, signal, file ops
Week 6: IPC syscalls (24) - POSIX + SysV IPC
Week 7: Memory & threading (18) - mmap, clone, futex
Week 8: Networking & timers (28) - Sockets, POSIX timers
------------------------------------------------------
Total: 130 new syscalls (plus 110 from existing enhancements)
```

---

## 2. System Call Architecture

### 2.1 Syscall Entry Path

**User Space → Kernel:**
```
1. User calls libc function (e.g., read())
2. musl libc wrapper invokes syscall instruction
3. CPU switches to kernel mode
4. syscall_entry handler (assembly)
5. syscall_dispatch() (C++)
6. sys_read() implementation
7. Return to user space via sysretq
```

**File Locations:**
- Entry: `src/kernel/arch/x86_64/syscall_entry.S`
- Dispatch: `src/kernel/syscall.cpp`
- Implementations: `src/kernel/syscall_*.cpp` (by category)

### 2.2 Syscall Convention (x86_64)

**Linux-compatible ABI:**

| Register | Purpose |
|----------|---------|
| RAX | Syscall number (input), Return value (output) |
| RDI | Argument 1 |
| RSI | Argument 2 |
| RDX | Argument 3 |
| R10 | Argument 4 (RCX clobbered by syscall) |
| R8 | Argument 5 |
| R9 | Argument 6 |
| RCX | Return RIP (clobbered) |
| R11 | RFLAGS (clobbered) |

**Return Convention:**
- Success: RAX = return value (≥ 0)
- Error: RAX = -errno (e.g., -ENOENT = -2)

### 2.3 Syscall Table Structure

```cpp
// include/xinim/kernel/syscall.hpp
namespace xinim::kernel {
    enum class Syscall : uint64_t {
        READ = 0,
        WRITE = 1,
        OPEN = 2,
        // ... 297 more
        MAX_SYSCALL = 299
    };

    struct SyscallEntry {
        using Handler = int64_t (*)(uint64_t, uint64_t, uint64_t,
                                     uint64_t, uint64_t, uint64_t);
        Handler handler;
        const char* name;
        uint8_t arg_count;
        bool needs_capability;
    };

    extern const SyscallEntry syscall_table[300];
}
```

---

## 3. Syscall Implementation Pattern

### 3.1 Basic Template

```cpp
// src/kernel/syscall_<category>.cpp
namespace xinim::kernel::syscall {

// Example: read(int fd, void* buf, size_t count)
auto sys_read(uint64_t fd_arg, uint64_t buf_arg, uint64_t count_arg,
              uint64_t, uint64_t, uint64_t) -> int64_t {
    // 1. Validate arguments
    auto fd = static_cast<int>(fd_arg);
    auto* buf = reinterpret_cast<void*>(buf_arg);
    auto count = static_cast<size_t>(count_arg);

    if (fd < 0) return -EBADF;
    if (!buf) return -EFAULT;
    if (count > SSIZE_MAX) return -EINVAL;

    // 2. Check user memory accessibility
    if (!mm::check_user_buffer(buf, count, MM_WRITABLE)) {
        return -EFAULT;
    }

    // 3. Get current process
    auto* proc = Process::current();
    if (!proc) return -ESRCH;

    // 4. Get file descriptor
    auto* file = proc->fd_table().get(fd);
    if (!file) return -EBADF;

    // 5. Perform operation
    auto result = file->read(buf, count);
    if (!result) {
        return -result.error().code();  // Convert error to -errno
    }

    // 6. Return success
    return static_cast<int64_t>(result.value());
}

} // namespace xinim::kernel::syscall
```

### 3.2 Error Handling

**Error Codes (from `<errno.h>`):**

| Error | Value | Meaning |
|-------|-------|---------|
| EPERM | 1 | Operation not permitted |
| ENOENT | 2 | No such file or directory |
| ESRCH | 3 | No such process |
| EINTR | 4 | Interrupted system call |
| EIO | 5 | I/O error |
| EBADF | 9 | Bad file descriptor |
| ENOMEM | 12 | Out of memory |
| EACCES | 13 | Permission denied |
| EFAULT | 14 | Bad address |
| EINVAL | 22 | Invalid argument |
| ENOSYS | 38 | Function not implemented |

**Return Pattern:**
```cpp
// Success
return result_value;  // >= 0

// Error
return -EINVAL;  // Negative errno
```

### 3.3 User Memory Access

**CRITICAL: Always validate user pointers!**

```cpp
// Check if user buffer is accessible
auto check_user_buffer(const void* buf, size_t len, int flags) -> bool;

// Flags:
constexpr int MM_READABLE = 0x1;
constexpr int MM_WRITABLE = 0x2;

// Example:
if (!mm::check_user_buffer(buf, count, MM_WRITABLE)) {
    return -EFAULT;
}

// Safe copy from user space
auto copy_from_user(void* kernel_dst, const void* user_src, size_t len) -> Result<void>;

// Safe copy to user space
auto copy_to_user(void* user_dst, const void* kernel_src, size_t len) -> Result<void>;
```

**Why?** Prevents kernel exploitation via invalid pointers!

### 3.4 Registration

**Syscall Table Entry:**
```cpp
// src/kernel/syscall_table.cpp
const SyscallEntry syscall_table[300] = {
    [0]  = { sys_read, "read", 3, false },
    [1]  = { sys_write, "write", 3, false },
    [2]  = { sys_open, "open", 3, false },
    // ...
};
```

**Header Declaration:**
```cpp
// include/xinim/kernel/syscall.hpp
namespace xinim::kernel::syscall {
    auto sys_read(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t;
    auto sys_write(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t;
    // ...
}
```

---

## 4. Week 5: Core Syscalls (60 syscalls)

### 4.1 Process Management (20 syscalls)

**File:** `src/kernel/syscall_process.cpp`

#### vfork() - Fast fork for exec
```cpp
auto sys_vfork() -> int64_t {
    auto* parent = Process::current();

    // Create child with shared memory (fast)
    auto* child = parent->fork(/* copy_on_write */ true, /* share_mm */ true);
    if (!child) return -ENOMEM;

    // Suspend parent until child exec or exit
    parent->set_state(ProcessState::Sleeping);
    child->set_vfork_parent(parent);

    return child->pid();
}
```

#### execve() - Execute program
```cpp
auto sys_execve(const char* pathname, char* const argv[], char* const envp[]) -> int64_t {
    // Validate user pointers
    if (!check_user_string(pathname)) return -EFAULT;
    if (!check_user_argv(argv)) return -EFAULT;
    if (!check_user_argv(envp)) return -EFAULT;

    // Load ELF binary
    auto elf = ElfLoader::load(pathname);
    if (!elf) return -elf.error().code();

    // Replace current process image
    auto* proc = Process::current();
    proc->exec(elf.value(), argv, envp);

    // Never returns to caller
    __builtin_unreachable();
}
```

#### Process Groups and Sessions
```cpp
auto sys_setpgid(pid_t pid, pid_t pgid) -> int64_t {
    auto* proc = (pid == 0) ? Process::current() : Process::find(pid);
    if (!proc) return -ESRCH;

    if (pgid == 0) pgid = proc->pid();

    proc->set_process_group(pgid);
    return 0;
}

auto sys_getpgid(pid_t pid) -> int64_t {
    auto* proc = (pid == 0) ? Process::current() : Process::find(pid);
    if (!proc) return -ESRCH;

    return proc->process_group();
}

auto sys_setsid() -> int64_t {
    auto* proc = Process::current();

    // Cannot create session if already session leader
    if (proc->pid() == proc->session_id()) return -EPERM;

    // Create new session
    proc->set_session_id(proc->pid());
    proc->set_process_group(proc->pid());
    proc->set_controlling_terminal(nullptr);

    return proc->session_id();
}
```

#### Priority and Scheduling
```cpp
auto sys_setpriority(int which, id_t who, int prio) -> int64_t {
    if (prio < -20 || prio > 19) return -EINVAL;

    switch (which) {
    case PRIO_PROCESS: {
        auto* proc = (who == 0) ? Process::current() : Process::find(who);
        if (!proc) return -ESRCH;
        proc->set_nice(prio);
        break;
    }
    case PRIO_PGRP:
    case PRIO_USER:
        // TODO: Implement group/user priority
        return -ENOSYS;
    default:
        return -EINVAL;
    }

    return 0;
}
```

### 4.2 Signal Handling (15 syscalls)

**File:** `src/kernel/syscall_signal.cpp`

#### sigaction() - Set signal handler
```cpp
auto sys_rt_sigaction(int signum, const struct sigaction* act,
                       struct sigaction* oldact, size_t sigsetsize) -> int64_t {
    if (sigsetsize != sizeof(sigset_t)) return -EINVAL;
    if (signum < 1 || signum > _NSIG) return -EINVAL;
    if (signum == SIGKILL || signum == SIGSTOP) return -EINVAL;

    auto* proc = Process::current();

    // Save old action
    if (oldact) {
        if (!check_user_buffer(oldact, sizeof(*oldact), MM_WRITABLE))
            return -EFAULT;

        auto old = proc->signal_handler(signum);
        copy_to_user(oldact, &old, sizeof(*oldact));
    }

    // Set new action
    if (act) {
        if (!check_user_buffer(act, sizeof(*act), MM_READABLE))
            return -EFAULT;

        struct sigaction new_act;
        copy_from_user(&new_act, act, sizeof(*act));
        proc->set_signal_handler(signum, new_act);
    }

    return 0;
}
```

#### Signal Masking
```cpp
auto sys_rt_sigprocmask(int how, const sigset_t* set, sigset_t* oldset,
                         size_t sigsetsize) -> int64_t {
    if (sigsetsize != sizeof(sigset_t)) return -EINVAL;

    auto* proc = Process::current();
    auto current_mask = proc->signal_mask();

    // Save old mask
    if (oldset) {
        if (!check_user_buffer(oldset, sizeof(*oldset), MM_WRITABLE))
            return -EFAULT;
        copy_to_user(oldset, &current_mask, sizeof(*oldset));
    }

    // Modify mask
    if (set) {
        if (!check_user_buffer(set, sizeof(*set), MM_READABLE))
            return -EFAULT;

        sigset_t new_set;
        copy_from_user(&new_set, set, sizeof(*set));

        switch (how) {
        case SIG_BLOCK:
            current_mask |= new_set;
            break;
        case SIG_UNBLOCK:
            current_mask &= ~new_set;
            break;
        case SIG_SETMASK:
            current_mask = new_set;
            break;
        default:
            return -EINVAL;
        }

        // Cannot block SIGKILL or SIGSTOP
        sigdelset(&current_mask, SIGKILL);
        sigdelset(&current_mask, SIGSTOP);

        proc->set_signal_mask(current_mask);
    }

    return 0;
}
```

### 4.3 File Operations (25 syscalls)

**File:** `src/kernel/syscall_file.cpp`

#### openat() - Open file relative to directory fd
```cpp
auto sys_openat(int dirfd, const char* pathname, int flags, mode_t mode) -> int64_t {
    if (!check_user_string(pathname)) return -EFAULT;

    auto* proc = Process::current();
    auto& vfs = VFS::instance();

    // Resolve path
    std::string path;
    if (pathname[0] == '/') {
        // Absolute path
        path = pathname;
    } else if (dirfd == AT_FDCWD) {
        // Relative to current directory
        path = proc->cwd() + "/" + pathname;
    } else {
        // Relative to dirfd
        auto* dir = proc->fd_table().get(dirfd);
        if (!dir) return -EBADF;
        if (!dir->is_directory()) return -ENOTDIR;
        path = dir->path() + "/" + pathname;
    }

    // Open file
    auto file = vfs.open(path, flags, mode);
    if (!file) return -file.error().code();

    // Allocate file descriptor
    auto fd = proc->fd_table().allocate(file.value());
    return fd;
}
```

#### mkdirat() - Create directory
```cpp
auto sys_mkdirat(int dirfd, const char* pathname, mode_t mode) -> int64_t {
    if (!check_user_string(pathname)) return -EFAULT;

    auto* proc = Process::current();
    auto& vfs = VFS::instance();

    // Resolve path (similar to openat)
    std::string path = resolve_path_at(dirfd, pathname);

    // Create directory
    auto result = vfs.mkdir(path, mode);
    if (!result) return -result.error().code();

    return 0;
}
```

#### linkat() - Create hard link
```cpp
auto sys_linkat(int olddirfd, const char* oldpath,
                int newdirfd, const char* newpath, int flags) -> int64_t {
    if (!check_user_string(oldpath)) return -EFAULT;
    if (!check_user_string(newpath)) return -EFAULT;

    auto& vfs = VFS::instance();

    std::string old_resolved = resolve_path_at(olddirfd, oldpath);
    std::string new_resolved = resolve_path_at(newdirfd, newpath);

    // Follow symlinks unless AT_SYMLINK_NOFOLLOW
    if (!(flags & AT_SYMLINK_NOFOLLOW)) {
        old_resolved = vfs.resolve_symlinks(old_resolved);
    }

    // Create hard link
    auto result = vfs.link(old_resolved, new_resolved);
    if (!result) return -result.error().code();

    return 0;
}
```

---

## 5. Week 6: IPC Syscalls (24 syscalls)

### 5.1 POSIX Shared Memory (4 syscalls)

**File:** `src/kernel/syscall_ipc_posix.cpp`

```cpp
auto sys_shm_open(const char* name, int oflag, mode_t mode) -> int64_t {
    if (!check_user_string(name)) return -EFAULT;
    if (name[0] != '/') return -EINVAL;

    // Shared memory is implemented as tmpfs files in /dev/shm
    auto path = std::format("/dev/shm{}", name);

    return sys_openat(AT_FDCWD, path.c_str(), oflag | O_CREAT, mode);
}

auto sys_shm_unlink(const char* name) -> int64_t {
    if (!check_user_string(name)) return -EFAULT;
    if (name[0] != '/') return -EINVAL;

    auto path = std::format("/dev/shm{}", name);

    return sys_unlinkat(AT_FDCWD, path.c_str(), 0);
}
```

### 5.2 POSIX Message Queues (8 syscalls)

**Fix for existing bug (97.22% → 100% compliance):**

```cpp
auto sys_mq_receive(mqd_t mqdes, char* msg_ptr, size_t msg_len,
                    unsigned* msg_prio) -> int64_t {
    if (!check_user_buffer(msg_ptr, msg_len, MM_WRITABLE)) return -EFAULT;
    if (msg_prio && !check_user_buffer(msg_prio, sizeof(*msg_prio), MM_WRITABLE))
        return -EFAULT;

    auto* mq = MessageQueue::from_descriptor(mqdes);
    if (!mq) return -EBADF;

    // FIXED: Proper timeout handling
    auto timeout = (mq->flags() & O_NONBLOCK) ?
        std::chrono::milliseconds(0) :
        std::chrono::milliseconds::max();

    auto result = mq->receive_with_timeout(msg_ptr, msg_len, msg_prio, timeout);
    if (!result) {
        if (result.error() == std::errc::timed_out) return -EAGAIN;
        return -result.error().code();
    }

    return result.value();
}
```

### 5.3 SysV IPC (12 syscalls)

**File:** `src/kernel/syscall_ipc_sysv.cpp`

#### SysV Message Queues
```cpp
namespace {
    std::map<key_t, SysVMsgQueue*> msg_queues;
    int next_msqid = 0;
}

auto sys_msgget(key_t key, int msgflg) -> int64_t {
    // Find existing queue
    if (key != IPC_PRIVATE) {
        auto it = msg_queues.find(key);
        if (it != msg_queues.end()) {
            if (msgflg & IPC_EXCL) return -EEXIST;
            return it->second->id();
        }
    }

    // Create new queue
    if (!(msgflg & IPC_CREAT)) return -ENOENT;

    auto* mq = new SysVMsgQueue(next_msqid++, key);
    msg_queues[key] = mq;

    return mq->id();
}

auto sys_msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg) -> int64_t {
    if (!check_user_buffer(msgp, msgsz + sizeof(long), MM_READABLE))
        return -EFAULT;

    auto* mq = find_msg_queue(msqid);
    if (!mq) return -EINVAL;

    // Extract message type (first long)
    long mtype;
    copy_from_user(&mtype, msgp, sizeof(long));

    // Extract message data
    std::vector<uint8_t> data(msgsz);
    copy_from_user(data.data(), (char*)msgp + sizeof(long), msgsz);

    // Send message
    auto result = mq->send(mtype, data, msgflg & IPC_NOWAIT);
    if (!result) return -result.error().code();

    return 0;
}
```

---

## 6. Week 7: Memory & Threading (18 syscalls)

### 6.1 Memory Management (10 syscalls)

**File:** `src/kernel/syscall_mman.cpp`

```cpp
auto sys_mprotect(void* addr, size_t len, int prot) -> int64_t {
    // Validate alignment
    if (!is_aligned(addr, PAGE_SIZE)) return -EINVAL;
    if (!is_aligned(len, PAGE_SIZE)) return -EINVAL;

    auto* proc = Process::current();
    auto* mm = proc->mm();

    // Find VMA (Virtual Memory Area)
    auto* vma = mm->find_vma(addr, len);
    if (!vma) return -ENOMEM;

    // Update protection
    auto result = vma->set_protection(prot);
    if (!result) return -result.error().code();

    // Flush TLB
    mm->flush_tlb(addr, len);

    return 0;
}

auto sys_mlock(const void* addr, size_t len) -> int64_t {
    // Pin pages in physical memory (prevent swapping)
    auto* proc = Process::current();
    auto* mm = proc->mm();

    auto result = mm->lock_pages(addr, len);
    if (!result) return -result.error().code();

    return 0;
}
```

### 6.2 Threading (8 syscalls)

**File:** `src/kernel/syscall_thread.cpp`

#### clone() - Create thread/process
```cpp
auto sys_clone(unsigned long flags, void* child_stack,
               int* parent_tidptr, int* child_tidptr,
               unsigned long tls) -> int64_t {
    auto* parent = Process::current();

    // Validate user pointers
    if (parent_tidptr && !check_user_buffer(parent_tidptr, sizeof(int), MM_WRITABLE))
        return -EFAULT;
    if (child_tidptr && !check_user_buffer(child_tidptr, sizeof(int), MM_WRITABLE))
        return -EFAULT;

    // Create thread or process
    Process* child;
    if (flags & CLONE_THREAD) {
        // Create thread (shares VM, files, signals)
        child = parent->create_thread({
            .stack = child_stack,
            .tls = tls,
            .parent_tid = parent_tidptr,
            .child_tid = child_tidptr,
            .flags = flags
        });
    } else {
        // Create process (fork-like)
        child = parent->fork(/* copy_on_write */ true);
        if (child_stack) {
            child->set_user_stack(child_stack);
        }
    }

    if (!child) return -ENOMEM;

    // Write TIDs
    if (parent_tidptr) copy_to_user(parent_tidptr, &child->tid(), sizeof(int));
    if (child_tidptr) copy_to_user(child_tidptr, &child->tid(), sizeof(int));

    // Set TLS
    if (tls && (flags & CLONE_SETTLS)) {
        child->set_tls(tls);
    }

    return child->tid();
}
```

#### futex() - Fast userspace mutex
```cpp
auto sys_futex(int* uaddr, int futex_op, int val,
               const struct timespec* timeout,
               int* uaddr2, int val3) -> int64_t {
    if (!check_user_buffer(uaddr, sizeof(int), MM_READABLE | MM_WRITABLE))
        return -EFAULT;

    int op = futex_op & FUTEX_CMD_MASK;
    bool is_private = futex_op & FUTEX_PRIVATE_FLAG;

    switch (op) {
    case FUTEX_WAIT: {
        // Atomic check and wait
        int current_val;
        copy_from_user(&current_val, uaddr, sizeof(int));

        if (current_val != val) return -EAGAIN;

        // Wait on futex
        auto result = Futex::wait(uaddr, val, timeout, is_private);
        if (!result) return -result.error().code();
        return 0;
    }

    case FUTEX_WAKE: {
        // Wake up waiters
        return Futex::wake(uaddr, val, is_private);
    }

    case FUTEX_REQUEUE: {
        // Requeue waiters to another futex
        if (!check_user_buffer(uaddr2, sizeof(int), MM_READABLE | MM_WRITABLE))
            return -EFAULT;
        return Futex::requeue(uaddr, val, val3, uaddr2, is_private);
    }

    default:
        return -ENOSYS;
    }
}
```

---

## 7. Week 8: Networking & Timers (28 syscalls)

### 7.1 Socket Operations (20 syscalls)

**File:** `src/kernel/syscall_socket.cpp`

```cpp
auto sys_socket(int domain, int type, int protocol) -> int64_t {
    auto* proc = Process::current();

    // Create socket
    auto sock = Socket::create(domain, type, protocol);
    if (!sock) return -sock.error().code();

    // Allocate file descriptor
    auto fd = proc->fd_table().allocate(sock.value());
    return fd;
}

auto sys_socketpair(int domain, int type, int protocol, int sv[2]) -> int64_t {
    if (!check_user_buffer(sv, 2 * sizeof(int), MM_WRITABLE))
        return -EFAULT;

    // Only UNIX domain sockets support socketpair
    if (domain != AF_UNIX) return -EOPNOTSUPP;

    auto* proc = Process::current();

    // Create pair
    auto pair = Socket::create_pair(domain, type, protocol);
    if (!pair) return -pair.error().code();

    // Allocate file descriptors
    int fds[2];
    fds[0] = proc->fd_table().allocate(pair.value().first);
    fds[1] = proc->fd_table().allocate(pair.value().second);

    // Copy to user
    copy_to_user(sv, fds, sizeof(fds));

    return 0;
}

auto sys_sendmsg(int sockfd, const struct msghdr* msg, int flags) -> int64_t {
    if (!check_user_buffer(msg, sizeof(*msg), MM_READABLE))
        return -EFAULT;

    auto* proc = Process::current();
    auto* sock = proc->fd_table().get_socket(sockfd);
    if (!sock) return -EBADF;

    // Copy msghdr from user
    struct msghdr kmsg;
    copy_from_user(&kmsg, msg, sizeof(*msg));

    // Validate iovec
    if (!check_user_buffer(kmsg.msg_iov, kmsg.msg_iovlen * sizeof(struct iovec), MM_READABLE))
        return -EFAULT;

    // Send message
    auto result = sock->sendmsg(&kmsg, flags);
    if (!result) return -result.error().code();

    return result.value();
}
```

### 7.2 POSIX Timers (8 syscalls)

**File:** `src/kernel/syscall_timer.cpp`

```cpp
auto sys_timer_create(clockid_t clockid, struct sigevent* sevp,
                       timer_t* timerid) -> int64_t {
    if (sevp && !check_user_buffer(sevp, sizeof(*sevp), MM_READABLE))
        return -EFAULT;
    if (!check_user_buffer(timerid, sizeof(*timerid), MM_WRITABLE))
        return -EFAULT;

    auto* proc = Process::current();

    // Copy sigevent
    struct sigevent event;
    if (sevp) {
        copy_from_user(&event, sevp, sizeof(event));
    } else {
        // Default: SIGALRM to creating process
        event.sigev_notify = SIGEV_SIGNAL;
        event.sigev_signo = SIGALRM;
    }

    // Create timer
    auto timer = PosixTimer::create(clockid, event);
    if (!timer) return -timer.error().code();

    auto id = proc->timer_table().allocate(timer.value());

    // Copy timer ID to user
    copy_to_user(timerid, &id, sizeof(id));

    return 0;
}

auto sys_timer_settime(timer_t timerid, int flags,
                        const struct itimerspec* new_value,
                        struct itimerspec* old_value) -> int64_t {
    if (!check_user_buffer(new_value, sizeof(*new_value), MM_READABLE))
        return -EFAULT;
    if (old_value && !check_user_buffer(old_value, sizeof(*old_value), MM_WRITABLE))
        return -EFAULT;

    auto* proc = Process::current();
    auto* timer = proc->timer_table().get(timerid);
    if (!timer) return -EINVAL;

    // Copy new value
    struct itimerspec new_spec;
    copy_from_user(&new_spec, new_value, sizeof(new_spec));

    // Save old value
    if (old_value) {
        auto old_spec = timer->spec();
        copy_to_user(old_value, &old_spec, sizeof(old_spec));
    }

    // Set new value
    auto result = timer->set_time(new_spec, flags & TIMER_ABSTIME);
    if (!result) return -result.error().code();

    return 0;
}
```

---

## 8. Testing

### 8.1 Unit Tests

**File:** `test/kernel/test_syscall.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <xinim/kernel/syscall.hpp>

TEST_CASE("Syscall: read/write", "[kernel][syscall]") {
    int pipefd[2];
    REQUIRE(pipe(pipefd) == 0);

    char write_buf[] = "Hello, XINIM!";
    char read_buf[64] = {0};

    // Write to pipe
    ssize_t written = write(pipefd[1], write_buf, sizeof(write_buf));
    REQUIRE(written == sizeof(write_buf));

    // Read from pipe
    ssize_t read_bytes = read(pipefd[0], read_buf, sizeof(read_buf));
    REQUIRE(read_bytes == sizeof(write_buf));
    REQUIRE(strcmp(read_buf, write_buf) == 0);

    close(pipefd[0]);
    close(pipefd[1]);
}

TEST_CASE("Syscall: fork/exec/wait", "[kernel][syscall]") {
    pid_t pid = fork();
    REQUIRE(pid >= 0);

    if (pid == 0) {
        // Child
        execlp("/bin/true", "true", nullptr);
        exit(1);  // Should not reach
    } else {
        // Parent
        int status;
        REQUIRE(waitpid(pid, &status, 0) == pid);
        REQUIRE(WIFEXITED(status));
        REQUIRE(WEXITSTATUS(status) == 0);
    }
}
```

### 8.2 Integration Tests

Run POSIX Test Suite:
```bash
cd third_party/gpl/posixtestsuite-main
./run_tests ALL > results.log
python3 ../../scripts/analyze_posix_results.py results.log
```

### 8.3 Syscall Stress Test

**File:** `test/stress/stress_syscall.cpp`

```cpp
// Stress test: Fork bomb (should be limited)
TEST_CASE("Stress: Fork bomb resistance", "[stress]") {
    int count = 0;
    while (fork() >= 0 && count < 1000) {
        count++;
    }
    // Should fail eventually (EAGAIN or ENOMEM)
    REQUIRE(count < 1000);
}

// Stress test: Rapid syscalls
TEST_CASE("Stress: Rapid getpid calls", "[stress]") {
    for (int i = 0; i < 1000000; i++) {
        getpid();
    }
    // Should not crash
}
```

---

## 9. Performance Optimization

### 9.1 Fast Path

**Optimize common syscalls:**
```cpp
// Fast path for read/write on pipes (no VFS overhead)
if (file->type() == FileType::Pipe) {
    return pipe_read_fast(file, buf, count);
}

// Fast path for getpid (no locks needed)
auto sys_getpid() -> int64_t {
    return Process::current()->pid();  // Thread-local, no lock
}
```

### 9.2 Caching

```cpp
// Cache frequently accessed structures
thread_local Process* current_process_cache = nullptr;
thread_local FileDescriptorTable* current_fd_table_cache = nullptr;
```

### 9.3 Batching

```cpp
// Batch multiple syscalls (e.g., readv/writev)
auto sys_readv(int fd, const struct iovec* iov, int iovcnt) -> int64_t {
    // Read multiple buffers in one syscall
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        auto result = read(fd, iov[i].iov_base, iov[i].iov_len);
        if (result < 0) return result;
        total += result;
        if (result < iov[i].iov_len) break;  // Short read
    }
    return total;
}
```

---

## 10. Checklist

### Week 5 Checklist
- [ ] Implement vfork, execve, wait4
- [ ] Implement setpgid, getpgid, setsid, getsid
- [ ] Implement sigaction, sigprocmask, sigsuspend
- [ ] Implement openat, mkdirat, linkat, symlinkat
- [ ] Test with POSIX test suite
- [ ] Achieve 100% compliance on process management

### Week 6 Checklist
- [ ] Implement shm_open, shm_unlink
- [ ] Fix mq_receive timeout bug (97.22% → 100%)
- [ ] Implement SysV IPC (msgget, semget, shmget families)
- [ ] Test IPC with multi-process programs
- [ ] Benchmark IPC performance

### Week 7 Checklist
- [ ] Implement mprotect, mlock, msync
- [ ] Implement clone (threads)
- [ ] Implement futex (critical for pthread)
- [ ] Implement set_tid_address, set_thread_area
- [ ] Test pthread applications
- [ ] Measure context switch latency

### Week 8 Checklist
- [ ] Implement socketpair, sendmsg, recvmsg
- [ ] Implement timer_create, timer_settime
- [ ] Test network applications (ping, wget)
- [ ] Test timer applications
- [ ] Full syscall table review

---

**Guide Status:** ✅ COMPLETE
**Ready for:** Week 5 implementation
**Next Update:** After Week 8 completion
