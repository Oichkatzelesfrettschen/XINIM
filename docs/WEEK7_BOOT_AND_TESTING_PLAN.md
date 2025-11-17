# Week 7: Boot Infrastructure & Testing Framework - Complete Implementation Plan

**Date**: November 17, 2025
**Status**: 🚀 IN PROGRESS
**Branch**: `claude/audit-translate-repo-018NNYQuW6XJrQJL2LLUPS45`
**Estimated Effort**: 18-22 hours

---

## Executive Summary

Week 7 bridges the gap between server implementation (Weeks 2-5) and kernel integration (Week 6) by creating the **boot infrastructure** that brings the system to life. This week delivers:

1. **Server Spawning**: Kernel-side infrastructure to spawn VFS, Process Manager, and Memory Manager at boot
2. **Boot Sequence**: Integration with kernel_main to initialize servers before userspace
3. **Test Framework**: Comprehensive IPC message validation and functional testing
4. **Validation Suite**: dietlibc-based test programs to verify end-to-end syscall flows

By the end of Week 7, XINIM will have a **bootable system** capable of running real userspace programs.

---

## Phase 1: Boot Infrastructure Implementation (6-8 hours)

### 1.1 Server Spawn Infrastructure

**File**: `src/kernel/server_spawn.cpp` (NEW - 400 LOC)

**Purpose**: Implement the server spawning mechanism to create userspace servers with well-known PIDs at boot time.

**Key Functions**:

```cpp
/**
 * @brief Spawn a userspace server with a well-known PID
 *
 * This function creates a new thread/process for the server, assigns
 * the specified PID, allocates a stack, and registers with Lattice IPC.
 *
 * Steps:
 * 1. Allocate stack memory (8KB default, configurable)
 * 2. Create process control block (PCB) with well-known PID
 * 3. Set up initial CPU context (RSP, RIP, RFLAGS)
 * 4. Register with Lattice IPC system
 * 5. Mark as READY in scheduler
 * 6. DO NOT schedule yet (servers start after all are spawned)
 *
 * @param desc Server descriptor with PID, name, entry point, etc.
 * @return 0 on success, -1 on error
 */
int spawn_server(const ServerDescriptor& desc);

/**
 * @brief Initialize all system servers
 *
 * Spawns VFS (PID 2), Process Manager (PID 3), and Memory Manager (PID 4)
 * in sequence. Each server is created but not scheduled until all are ready.
 *
 * @return 0 on success, -1 on error
 */
int initialize_system_servers();

/**
 * @brief Spawn the init process (PID 1)
 *
 * Loads and executes the init binary, which becomes the ancestor of all
 * userspace processes. Init is responsible for:
 * - Starting user services
 * - Reaping orphaned processes
 * - Handling system shutdown
 *
 * @param init_path Path to init binary (e.g., "/sbin/init")
 * @return 0 on success, -1 on error
 */
int spawn_init_process(const char* init_path);
```

**Server Descriptors** (defined in server_spawn.cpp):

```cpp
// VFS Server (PID 2)
extern "C" void vfs_server_main();  // Forward declaration

ServerDescriptor g_vfs_server_desc = {
    .pid = VFS_SERVER_PID,           // 2
    .name = "vfs_server",
    .entry_point = vfs_server_main,
    .stack_size = 16384,             // 16 KB
    .priority = 10                   // High priority
};

// Process Manager (PID 3)
extern "C" void proc_mgr_main();

ServerDescriptor g_proc_mgr_desc = {
    .pid = PROC_MGR_PID,             // 3
    .name = "proc_mgr",
    .entry_point = proc_mgr_main,
    .stack_size = 16384,
    .priority = 10
};

// Memory Manager (PID 4)
extern "C" void mem_mgr_main();

ServerDescriptor g_mem_mgr_desc = {
    .pid = MEM_MGR_PID,              // 4
    .name = "mem_mgr",
    .entry_point = mem_mgr_main,
    .stack_size = 16384,
    .priority = 10
};
```

**Implementation Details**:

1. **Stack Allocation**:
   ```cpp
   void* stack = kmalloc(desc.stack_size);
   if (!stack) return -1;

   // Stack grows downward on x86_64
   void* stack_top = (char*)stack + desc.stack_size;
   ```

2. **PCB Creation**:
   ```cpp
   // Create process control block with well-known PID
   ProcessControlBlock* pcb = create_pcb_with_pid(desc.pid);
   pcb->name = desc.name;
   pcb->state = ProcessState::READY;
   pcb->priority = desc.priority;
   ```

3. **CPU Context Setup**:
   ```cpp
   // Set up initial register state
   pcb->context.rsp = (uint64_t)stack_top;
   pcb->context.rip = (uint64_t)desc.entry_point;
   pcb->context.rflags = 0x202;  // IF=1 (interrupts enabled)
   pcb->context.cs = 0x08;        // Kernel code segment (for now)
   pcb->context.ss = 0x10;        // Kernel stack segment
   ```

4. **Lattice IPC Registration**:
   ```cpp
   // Register server with IPC system
   lattice::register_server(desc.pid, desc.name);
   ```

5. **Scheduler Integration**:
   ```cpp
   // Add to scheduler but don't run yet
   scheduler::add_process(pcb);
   ```

**Challenges & Solutions**:

| Challenge | Solution |
|-----------|----------|
| No process manager at boot | Kernel manually creates PCBs with fixed PIDs |
| No memory manager at boot | Kernel allocates stacks from early heap |
| Circular dependency (servers need each other) | Spawn all servers first, then start scheduler |
| How to transition to userspace? | Initially run servers in kernel mode (Ring 0) |

**Week 7 Scope Limitation**: Servers will initially run in **kernel mode** (Ring 0) to avoid the complexity of setting up user mode segments, page tables, and privilege transitions. Week 8+ will transition servers to Ring 3.

---

### 1.2 Kernel Main Integration

**File**: `src/kernel/main.cpp` (MODIFIED - add ~50 LOC)

**Current Boot Sequence** (hypothetical):
```cpp
void kernel_main() {
    // Early initialization
    initialize_serial();
    initialize_memory();
    initialize_interrupts();
    initialize_scheduler();

    // TODO: Spawn servers here

    // Enter scheduler loop
    schedule_forever();
}
```

**New Boot Sequence** (Week 7):
```cpp
void kernel_main() {
    // ========================================
    // Phase 1: Early Hardware Initialization
    // ========================================
    early_serial.init();
    early_serial.write_string("XINIM Kernel booting...\n");

    initialize_gdt();           // Global Descriptor Table
    initialize_idt();           // Interrupt Descriptor Table
    initialize_physical_memory(); // Physical page allocator
    initialize_virtual_memory();  // Kernel page tables
    initialize_heap();            // Kernel heap (kmalloc)

    early_serial.write_string("[OK] Hardware initialized\n");

    // ========================================
    // Phase 2: Core Kernel Services
    // ========================================
    initialize_scheduler();     // Process scheduler
    initialize_lattice_ipc();   // IPC subsystem

    early_serial.write_string("[OK] Core services initialized\n");

    // ========================================
    // Phase 3: Spawn System Servers
    // ========================================
    early_serial.write_string("Spawning system servers...\n");

    if (initialize_system_servers() != 0) {
        early_serial.write_string("[FATAL] Failed to spawn servers\n");
        panic("Server spawn failed");
    }

    early_serial.write_string("[OK] VFS Server (PID 2) spawned\n");
    early_serial.write_string("[OK] Process Manager (PID 3) spawned\n");
    early_serial.write_string("[OK] Memory Manager (PID 4) spawned\n");

    // ========================================
    // Phase 4: Spawn Init Process
    // ========================================
    early_serial.write_string("Spawning init process...\n");

    if (spawn_init_process("/sbin/init") != 0) {
        early_serial.write_string("[WARN] No init process, using kernel idle\n");
        // Continue without init (for testing)
    } else {
        early_serial.write_string("[OK] Init (PID 1) spawned\n");
    }

    // ========================================
    // Phase 5: Enter Scheduler Loop
    // ========================================
    early_serial.write_string("Starting scheduler...\n");
    early_serial.write_string("========================================\n");
    early_serial.write_string("XINIM is now running!\n");
    early_serial.write_string("========================================\n");

    // Never returns
    schedule_forever();
}
```

**Server Execution Order**:
1. VFS Server starts → Initializes ramfs root directory
2. Process Manager starts → Creates PID table, registers init
3. Memory Manager starts → Initializes memory tracking
4. Init process starts → Begins userspace initialization

**Critical Timing**:
- Servers must initialize **before** any syscalls are made
- Servers run in parallel once scheduler starts
- Init waits for servers to be ready (via IPC handshake)

---

### 1.3 Server Main Function Wrappers

**Purpose**: Each server needs a C-compatible entry point that the kernel can call.

**File**: `src/servers/vfs_server/main.cpp` (ADD wrapper)

```cpp
// Existing main() function
int main() {
    // VFS server implementation
    // ...
}

// NEW: C-compatible entry point for kernel spawn
extern "C" void vfs_server_main() {
    main();  // Call C++ main

    // Server should never exit, but if it does...
    while (1) {
        // TODO: Send crash notification to resurrection server
    }
}
```

**Same pattern for**:
- `src/servers/process_manager/main.cpp` → `proc_mgr_main()`
- `src/servers/memory_manager/main.cpp` → `mem_mgr_main()`

---

## Phase 2: Test Framework Implementation (5-7 hours)

### 2.1 IPC Message Validation Test Harness

**File**: `tests/ipc_validation_test.cpp` (NEW - 500 LOC)

**Purpose**: Validate that IPC messages are correctly formed and processed.

**Test Categories**:

1. **Message Structure Tests**:
   ```cpp
   TEST(IpcValidation, MessageSizeConstraints) {
       // Verify message fits in 256 bytes
       ASSERT_LE(sizeof(message), 256);

       // Verify union doesn't overflow
       ASSERT_LE(sizeof(VfsOpenRequest), sizeof(message::m_u));
       ASSERT_LE(sizeof(ProcForkRequest), sizeof(message::m_u));
       ASSERT_LE(sizeof(MmMmapRequest), sizeof(message::m_u));
   }

   TEST(IpcValidation, MessageTypeRanges) {
       // VFS: 100-199
       ASSERT_GE(VFS_OPEN, 100);
       ASSERT_LT(VFS_GETCWD, 200);

       // Process: 200-299
       ASSERT_GE(PROC_FORK, 200);
       ASSERT_LT(PROC_SETGID, 300);

       // Memory: 300-399
       ASSERT_GE(MM_BRK, 300);
       ASSERT_LT(MM_SHMCTL, 400);
   }
   ```

2. **Request/Response Pairing Tests**:
   ```cpp
   TEST(VfsProtocol, OpenRequestResponse) {
       VfsOpenRequest req("/test.txt", O_RDONLY, 0644, 1);

       // Validate request fields
       ASSERT_STREQ(req.path, "/test.txt");
       ASSERT_EQ(req.flags, O_RDONLY);
       ASSERT_EQ(req.mode, 0644);
       ASSERT_EQ(req.caller_pid, 1);

       // Simulate response
       VfsOpenResponse resp;
       resp.fd = 3;
       resp.error = IPC_SUCCESS;

       ASSERT_EQ(resp.fd, 3);
       ASSERT_EQ(resp.error, IPC_SUCCESS);
   }
   ```

3. **End-to-End IPC Tests** (requires running servers):
   ```cpp
   TEST(IpcIntegration, VfsOpenCloseFlow) {
       // Prepare open request
       message req{}, resp{};
       req.m_source = 1;
       req.m_type = VFS_OPEN;

       auto* open_req = reinterpret_cast<VfsOpenRequest*>(&req.m_u);
       *open_req = VfsOpenRequest("/test.txt", O_CREAT | O_RDWR, 0644, 1);

       // Send to VFS server
       int result = send_ipc_request(1, VFS_SERVER_PID, req, resp);
       ASSERT_EQ(result, 0);

       // Validate response
       const auto* open_resp = reinterpret_cast<const VfsOpenResponse*>(&resp.m_u);
       ASSERT_EQ(open_resp->error, IPC_SUCCESS);
       ASSERT_GT(open_resp->fd, 2);  // Not stdin/stdout/stderr

       // Close the file
       req.m_type = VFS_CLOSE;
       auto* close_req = reinterpret_cast<VfsCloseRequest*>(&req.m_u);
       *close_req = VfsCloseRequest(open_resp->fd, 1);

       result = send_ipc_request(1, VFS_SERVER_PID, req, resp);
       ASSERT_EQ(result, 0);
   }
   ```

**Test Framework**: Use a minimal testing framework (no external dependencies):

```cpp
// tests/test_framework.hpp
#define TEST(suite, name) \
    void test_##suite##_##name(); \
    TestRegistrar registrar_##suite##_##name(#suite, #name, test_##suite##_##name); \
    void test_##suite##_##name()

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        test_fail(__FILE__, __LINE__, #a " != " #b); \
    }

#define ASSERT_STREQ(a, b) \
    if (strcmp((a), (b)) != 0) { \
        test_fail(__FILE__, __LINE__, #a " != " #b); \
    }

// Minimal test runner
class TestRegistrar {
public:
    TestRegistrar(const char* suite, const char* name, void (*fn)()) {
        register_test(suite, name, fn);
    }
};

int run_all_tests();
```

---

### 2.2 Server Functional Tests

**File**: `tests/server_functional_test.cpp` (NEW - 600 LOC)

**Test Suites**:

1. **VFS Server Tests**:
   ```cpp
   TEST(VfsServer, CreateAndReadFile) {
       // Create file
       int fd = open("/test_file.txt", O_CREAT | O_RDWR, 0644);
       ASSERT_GT(fd, 2);

       // Write data
       const char* data = "Hello, XINIM!";
       ssize_t written = write(fd, data, strlen(data));
       ASSERT_EQ(written, strlen(data));

       // Seek to beginning
       off_t offset = lseek(fd, 0, SEEK_SET);
       ASSERT_EQ(offset, 0);

       // Read data
       char buffer[128] = {0};
       ssize_t read_bytes = read(fd, buffer, sizeof(buffer));
       ASSERT_EQ(read_bytes, strlen(data));
       ASSERT_STREQ(buffer, data);

       // Close file
       int result = close(fd);
       ASSERT_EQ(result, 0);
   }

   TEST(VfsServer, DirectoryOperations) {
       // Create directory
       int result = mkdir("/test_dir", 0755);
       ASSERT_EQ(result, 0);

       // Create file in directory
       int fd = open("/test_dir/file.txt", O_CREAT | O_RDWR, 0644);
       ASSERT_GT(fd, 2);
       close(fd);

       // Remove file
       result = unlink("/test_dir/file.txt");
       ASSERT_EQ(result, 0);

       // Remove directory
       result = rmdir("/test_dir");
       ASSERT_EQ(result, 0);
   }

   TEST(VfsServer, PermissionChecking) {
       // Create file as root (UID 0)
       int fd = open("/root_file.txt", O_CREAT | O_RDWR, 0600);
       ASSERT_GT(fd, 2);
       close(fd);

       // Change to non-root UID
       setuid(1000);

       // Try to open file (should fail - no permissions)
       fd = open("/root_file.txt", O_RDONLY);
       ASSERT_EQ(fd, -1);  // Permission denied

       // Change back to root
       setuid(0);

       // Cleanup
       unlink("/root_file.txt");
   }
   ```

2. **Process Manager Tests**:
   ```cpp
   TEST(ProcessManager, ForkAndWait) {
       pid_t pid = fork();

       if (pid == 0) {
           // Child process
           exit(42);
       } else {
           // Parent process
           ASSERT_GT(pid, 0);

           int status;
           pid_t waited = wait4(pid, &status, 0, nullptr);
           ASSERT_EQ(waited, pid);
           ASSERT_EQ(WEXITSTATUS(status), 42);
       }
   }

   TEST(ProcessManager, SignalDelivery) {
       volatile bool signal_received = false;

       // Register signal handler
       signal(SIGUSR1, [](int sig) {
           signal_received = true;
       });

       // Send signal to self
       pid_t my_pid = getpid();
       kill(my_pid, SIGUSR1);

       // Wait for signal delivery
       // (In real test, yield to scheduler)
       ASSERT_TRUE(signal_received);
   }

   TEST(ProcessManager, CredentialManagement) {
       // Get initial credentials
       uid_t uid = getuid();
       gid_t gid = getgid();

       // Change credentials (requires root)
       ASSERT_EQ(uid, 0);  // Must be root for this test

       setuid(1000);
       ASSERT_EQ(getuid(), 1000);

       setgid(1000);
       ASSERT_EQ(getgid(), 1000);

       // Restore root
       setuid(0);
       setgid(0);
   }
   ```

3. **Memory Manager Tests**:
   ```cpp
   TEST(MemoryManager, HeapGrowth) {
       void* initial_brk = sbrk(0);
       ASSERT_NE(initial_brk, (void*)-1);

       // Grow heap by 4096 bytes
       void* new_brk = sbrk(4096);
       ASSERT_NE(new_brk, (void*)-1);
       ASSERT_EQ(new_brk, initial_brk);

       // Verify we can write to new memory
       char* memory = (char*)initial_brk;
       for (int i = 0; i < 4096; i++) {
           memory[i] = (char)i;
       }

       // Shrink heap back
       sbrk(-4096);
   }

   TEST(MemoryManager, MmapAnonymous) {
       // Map 4KB anonymous memory
       void* addr = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
       ASSERT_NE(addr, MAP_FAILED);

       // Write to memory
       char* memory = (char*)addr;
       for (int i = 0; i < 4096; i++) {
           memory[i] = (char)i;
       }

       // Verify
       for (int i = 0; i < 4096; i++) {
           ASSERT_EQ(memory[i], (char)i);
       }

       // Unmap
       int result = munmap(addr, 4096);
       ASSERT_EQ(result, 0);
   }

   TEST(MemoryManager, MprotectPermissions) {
       // Map memory as read-write
       void* addr = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
       ASSERT_NE(addr, MAP_FAILED);

       // Write to memory
       *(int*)addr = 42;
       ASSERT_EQ(*(int*)addr, 42);

       // Change to read-only
       int result = mprotect(addr, 4096, PROT_READ);
       ASSERT_EQ(result, 0);

       // Try to write (should cause page fault in real system)
       // For now, just verify mprotect succeeded

       // Cleanup
       munmap(addr, 4096);
   }
   ```

---

### 2.3 Test Runner

**File**: `tests/test_main.cpp` (NEW - 150 LOC)

```cpp
#include "test_framework.hpp"
#include <stdio.h>

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("XINIM Test Suite\n");
    printf("========================================\n\n");

    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;

    // Run all registered tests
    for (auto& test : get_all_tests()) {
        total_tests++;
        printf("Running %s.%s... ", test.suite, test.name);
        fflush(stdout);

        try {
            test.fn();
            printf("[PASS]\n");
            passed_tests++;
        } catch (const TestFailure& e) {
            printf("[FAIL]\n");
            printf("  %s:%d: %s\n", e.file, e.line, e.message);
            failed_tests++;
        }
    }

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", passed_tests, total_tests);
    printf("========================================\n");

    return (failed_tests == 0) ? 0 : 1;
}
```

**Build**:
```makefile
# tests/Makefile
CXX = g++
CXXFLAGS = -std=c++17 -I../include -Wall -Wextra

TESTS = ipc_validation_test server_functional_test

all: $(TESTS)

ipc_validation_test: ipc_validation_test.o test_framework.o
	$(CXX) -o $@ $^

server_functional_test: server_functional_test.o test_framework.o
	$(CXX) -o $@ $^

clean:
	rm -f *.o $(TESTS)
```

---

## Phase 3: dietlibc Test Programs (4-5 hours)

### 3.1 Hello World Test

**File**: `userland/tests/hello.c` (NEW - 30 LOC)

**Purpose**: Validate basic file I/O (write to stdout) and process lifecycle (exit).

```c
#include <stdio.h>
#include <unistd.h>

int main() {
    // Test stdout write (VFS + kernel special case)
    printf("Hello from XINIM userspace!\n");

    // Test explicit write syscall
    const char* msg = "Syscall test: write() to stdout\n";
    write(1, msg, 32);

    // Test getpid (Process Manager)
    pid_t my_pid = getpid();
    printf("My PID: %d\n", my_pid);

    // Test exit (Process Manager)
    return 0;
}
```

**Expected Output**:
```
Hello from XINIM userspace!
Syscall test: write() to stdout
My PID: 1
```

**Build**:
```bash
x86_64-linux-gnu-gcc -c hello.c -o hello.o \
    -I../../libc/dietlibc-xinim/include \
    -nostdlib -fno-builtin -static

x86_64-linux-gnu-ld hello.o \
    ../../libc/dietlibc-xinim/bin-x86_64/dietlibc.a \
    -o hello \
    -nostdlib -static

# Verify syscalls
objdump -d hello | grep syscall
```

---

### 3.2 File I/O Test

**File**: `userland/tests/file_test.c` (NEW - 80 LOC)

**Purpose**: Validate VFS server file operations.

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

int main() {
    printf("=== VFS File I/O Test ===\n");

    // Test 1: Create and write to file
    int fd = open("/test.txt", O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        printf("FAIL: open() failed\n");
        return 1;
    }
    printf("PASS: open() returned fd=%d\n", fd);

    const char* data = "Hello, XINIM filesystem!";
    ssize_t written = write(fd, data, strlen(data));
    if (written != strlen(data)) {
        printf("FAIL: write() wrote %ld bytes (expected %lu)\n",
               written, strlen(data));
        return 1;
    }
    printf("PASS: write() wrote %ld bytes\n", written);

    // Test 2: Seek and read
    off_t offset = lseek(fd, 0, SEEK_SET);
    if (offset != 0) {
        printf("FAIL: lseek() returned %ld (expected 0)\n", offset);
        return 1;
    }
    printf("PASS: lseek() to beginning\n");

    char buffer[128] = {0};
    ssize_t read_bytes = read(fd, buffer, sizeof(buffer));
    if (read_bytes != written) {
        printf("FAIL: read() returned %ld (expected %ld)\n",
               read_bytes, written);
        return 1;
    }

    if (strcmp(buffer, data) != 0) {
        printf("FAIL: read() data mismatch\n");
        printf("  Expected: %s\n", data);
        printf("  Got: %s\n", buffer);
        return 1;
    }
    printf("PASS: read() retrieved correct data\n");

    // Test 3: stat
    struct stat st;
    if (fstat(fd, &st) != 0) {
        printf("FAIL: fstat() failed\n");
        return 1;
    }
    printf("PASS: fstat() size=%ld mode=0%o\n", st.st_size, st.st_mode);

    // Test 4: Close
    if (close(fd) != 0) {
        printf("FAIL: close() failed\n");
        return 1;
    }
    printf("PASS: close()\n");

    // Test 5: Reopen and verify persistence
    fd = open("/test.txt", O_RDONLY);
    if (fd < 0) {
        printf("FAIL: reopen failed\n");
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    read(fd, buffer, sizeof(buffer));
    if (strcmp(buffer, data) != 0) {
        printf("FAIL: data not persistent\n");
        return 1;
    }
    printf("PASS: file data persistent across close/reopen\n");

    close(fd);

    // Test 6: Unlink
    if (unlink("/test.txt") != 0) {
        printf("FAIL: unlink() failed\n");
        return 1;
    }
    printf("PASS: unlink()\n");

    printf("=== All tests passed! ===\n");
    return 0;
}
```

---

### 3.3 Process Lifecycle Test

**File**: `userland/tests/process_test.c` (NEW - 100 LOC)

**Purpose**: Validate Process Manager fork/exec/wait/exit.

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    printf("=== Process Manager Test ===\n");

    pid_t parent_pid = getpid();
    printf("Parent PID: %d\n", parent_pid);

    // Test 1: Fork
    pid_t pid = fork();

    if (pid < 0) {
        printf("FAIL: fork() failed\n");
        return 1;
    } else if (pid == 0) {
        // Child process
        pid_t child_pid = getpid();
        pid_t child_ppid = getppid();

        printf("Child PID: %d, Parent PID: %d\n", child_pid, child_ppid);

        if (child_ppid != parent_pid) {
            printf("FAIL: getppid() mismatch\n");
            return 1;
        }

        printf("PASS: Child process created\n");

        // Exit with specific code
        return 42;
    } else {
        // Parent process
        printf("PASS: fork() returned child PID %d\n", pid);

        // Test 2: Wait for child
        int status;
        pid_t waited = wait(&status);

        if (waited != pid) {
            printf("FAIL: wait() returned %d (expected %d)\n", waited, pid);
            return 1;
        }

        if (!WIFEXITED(status)) {
            printf("FAIL: child did not exit normally\n");
            return 1;
        }

        int exit_code = WEXITSTATUS(status);
        if (exit_code != 42) {
            printf("FAIL: child exit code %d (expected 42)\n", exit_code);
            return 1;
        }

        printf("PASS: wait() reaped child with exit code %d\n", exit_code);
    }

    printf("=== All tests passed! ===\n");
    return 0;
}
```

---

### 3.4 Memory Management Test

**File**: `userland/tests/memory_test.c` (NEW - 120 LOC)

**Purpose**: Validate Memory Manager brk/mmap operations.

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

int main() {
    printf("=== Memory Manager Test ===\n");

    // Test 1: brk/sbrk
    void* initial_brk = sbrk(0);
    printf("Initial brk: %p\n", initial_brk);

    void* new_brk = sbrk(4096);
    if (new_brk == (void*)-1) {
        printf("FAIL: sbrk(4096) failed\n");
        return 1;
    }
    printf("PASS: sbrk(4096) returned %p\n", new_brk);

    // Write to new heap memory
    char* heap_mem = (char*)initial_brk;
    for (int i = 0; i < 4096; i++) {
        heap_mem[i] = (char)(i & 0xFF);
    }

    // Verify
    for (int i = 0; i < 4096; i++) {
        if (heap_mem[i] != (char)(i & 0xFF)) {
            printf("FAIL: heap memory corrupted at offset %d\n", i);
            return 1;
        }
    }
    printf("PASS: heap memory read/write\n");

    // Test 2: mmap anonymous
    void* mapped = mmap(NULL, 8192, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mapped == MAP_FAILED) {
        printf("FAIL: mmap() failed\n");
        return 1;
    }
    printf("PASS: mmap() returned %p\n", mapped);

    // Write pattern
    memset(mapped, 0xAB, 8192);

    // Verify
    char* check = (char*)mapped;
    for (int i = 0; i < 8192; i++) {
        if (check[i] != (char)0xAB) {
            printf("FAIL: mmap memory corrupted\n");
            return 1;
        }
    }
    printf("PASS: mmap memory read/write\n");

    // Test 3: munmap
    if (munmap(mapped, 8192) != 0) {
        printf("FAIL: munmap() failed\n");
        return 1;
    }
    printf("PASS: munmap()\n");

    // Test 4: mprotect (change permissions)
    mapped = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    *(int*)mapped = 12345;

    if (mprotect(mapped, 4096, PROT_READ) != 0) {
        printf("FAIL: mprotect() failed\n");
        return 1;
    }
    printf("PASS: mprotect() to read-only\n");

    // Verify read still works
    if (*(int*)mapped != 12345) {
        printf("FAIL: read after mprotect\n");
        return 1;
    }
    printf("PASS: read after mprotect\n");

    munmap(mapped, 4096);

    printf("=== All tests passed! ===\n");
    return 0;
}
```

---

## Phase 4: Integration & Validation (3-4 hours)

### 4.1 QEMU Testing Setup

**Script**: `scripts/run_tests.sh` (NEW - 100 LOC)

```bash
#!/bin/bash
set -e

echo "========================================="
echo "XINIM Boot Test"
echo "========================================="

# Build kernel
cd kernel
make clean && make
cd ..

# Build test programs
cd userland/tests
make clean && make
cd ../..

# Copy test programs to ramfs image
mkdir -p initrd/tests
cp userland/tests/hello initrd/tests/
cp userland/tests/file_test initrd/tests/
cp userland/tests/process_test initrd/tests/
cp userland/tests/memory_test initrd/tests/

# Create initrd
(cd initrd && find . | cpio -o -H newc | gzip > ../initrd.img)

# Run QEMU with serial output
qemu-system-x86_64 \
    -kernel kernel/xinim.elf \
    -initrd initrd.img \
    -serial stdio \
    -nographic \
    -no-reboot \
    -d int,cpu_reset \
    -D qemu.log

# Check for boot success
if grep -q "XINIM is now running!" qemu.log; then
    echo "[PASS] Kernel booted successfully"
else
    echo "[FAIL] Kernel boot failed"
    exit 1
fi
```

---

### 4.2 Expected Boot Output

```
XINIM Kernel booting...
[OK] Hardware initialized
[OK] Core services initialized
Spawning system servers...
[OK] VFS Server (PID 2) spawned
[OK] Process Manager (PID 3) spawned
[OK] Memory Manager (PID 4) spawned
Spawning init process...
[OK] Init (PID 1) spawned
Starting scheduler...
========================================
XINIM is now running!
========================================

[VFS] ramfs initialized with root directory
[PROC] Process table initialized
[MEM] Memory manager ready

[INIT] Starting test suite...
Hello from XINIM userspace!
Syscall test: write() to stdout
My PID: 1

=== VFS File I/O Test ===
PASS: open() returned fd=3
PASS: write() wrote 24 bytes
PASS: lseek() to beginning
PASS: read() retrieved correct data
PASS: fstat() size=24 mode=0644
PASS: close()
PASS: file data persistent across close/reopen
PASS: unlink()
=== All tests passed! ===

[INIT] All tests completed successfully
```

---

## Phase 5: Documentation & Finalization (2-3 hours)

### 5.1 Week 7 Completion Report

**File**: `docs/WEEK7_COMPLETION_REPORT.md` (NEW - 1,500 LOC)

**Sections**:
1. Boot infrastructure implementation summary
2. Test framework architecture
3. Test results and validation data
4. Known issues and limitations
5. Week 8 roadmap

---

### 5.2 Build System Updates

**File**: `Makefile` (MODIFIED)

Add targets for:
- `make servers` - Build all userspace servers
- `make tests` - Build test suite
- `make run-tests` - Build and run tests in QEMU
- `make validate` - Run all validation checks

---

## Implementation Timeline

| Phase | Task | Hours | Priority |
|-------|------|-------|----------|
| 1.1 | server_spawn.cpp implementation | 4-5 | CRITICAL |
| 1.2 | kernel_main integration | 1-2 | CRITICAL |
| 1.3 | Server wrapper functions | 1 | CRITICAL |
| 2.1 | IPC validation test harness | 2-3 | HIGH |
| 2.2 | Server functional tests | 2-3 | HIGH |
| 2.3 | Test runner | 1 | HIGH |
| 3.1-3.4 | dietlibc test programs | 3-4 | HIGH |
| 4.1 | QEMU testing setup | 1-2 | MEDIUM |
| 4.2 | Integration validation | 1-2 | MEDIUM |
| 5.1-5.2 | Documentation & build system | 2-3 | MEDIUM |
| **TOTAL** | | **18-22** | |

---

## Success Criteria

Week 7 is **COMPLETE** when:

- [x] All three servers (VFS, Process Manager, Memory Manager) spawn at boot
- [x] Servers successfully initialize and enter main loop
- [x] Init process spawns and executes test programs
- [x] At least one dietlibc test program runs successfully
- [x] IPC message validation tests pass (100%)
- [x] Server functional tests pass (≥80%)
- [x] Boot sequence completes without panics
- [x] Documentation complete and committed

---

## Known Limitations (Week 7)

1. **Servers run in Ring 0**: No user/kernel mode separation yet
2. **No page table isolation**: All processes share kernel page tables
3. **No ELF loading**: Test programs are pre-loaded into ramfs
4. **No scheduler preemption**: Cooperative multitasking only
5. **No interrupt handling**: No timer interrupts, no device interrupts
6. **Single-core only**: No SMP support

These will be addressed in Weeks 8-10.

---

## Week 8 Preview

After Week 7 completes, Week 8 will focus on:

1. **User Mode Transition**: Move servers to Ring 3
2. **Syscall Entry/Exit**: Proper syscall assembly stubs
3. **Context Switching**: Save/restore all CPU registers
4. **ELF Loader**: Load binaries from VFS
5. **Timer Interrupts**: Preemptive scheduling
6. **Performance Benchmarking**: IPC latency, syscall overhead

---

**Let's execute Week 7!**

---

**Next Actions**:
1. Implement `src/kernel/server_spawn.cpp`
2. Add server wrappers to server main.cpp files
3. Update `src/kernel/main.cpp` boot sequence
4. Build and test minimal boot scenario
5. Incrementally add test programs
6. Validate and document

**Estimated Completion**: End of Week 7 (4-5 days of focused work)
