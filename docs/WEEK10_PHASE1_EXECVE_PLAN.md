# Week 10 Phase 1: execve Implementation - Detailed Plan

**Date**: November 18, 2025
**Status**: 🚀 READY TO START
**Phase**: Week 10 Phase 1 - Program Execution
**Estimated Effort**: 8-10 hours
**Dependencies**: Week 9 Phase 3 complete (pipes, dup, fork)

---

## Executive Summary

Week 10 Phase 1 implements `execve`, the critical syscall that replaces a process's current program image with a new executable. This enables:
- Loading and executing arbitrary ELF binaries
- Shell command execution (via fork + execve)
- Process image replacement without creating new PIDs
- Proper handling of file descriptors across exec (CLOEXEC)

By the end of Phase 1, XINIM will be capable of executing userspace programs loaded from the filesystem, completing the fundamental process lifecycle (fork → exec → exit → wait).

---

## Technical Background

### execve System Call

**POSIX Signature**:
```c
int execve(const char *pathname, char *const argv[], char *const envp[]);
```

**Behavior**:
1. **Loads new program**: Parses ELF binary and loads into memory
2. **Replaces address space**: Frees old memory, sets up new mappings
3. **Preserves process identity**: PID, PPID, open FDs (except CLOEXEC) remain
4. **Resets state**: Signal handlers, timers, etc. reset to defaults
5. **Never returns on success**: Jumps directly to new program entry point
6. **Returns -errno on failure**: Old program continues if exec fails

### ELF Binary Format

**ELF (Executable and Linkable Format)**:
- Standard binary format on Unix/Linux
- Contains program code, data, metadata
- Supports static and dynamic linking
- Defines entry point, memory layout, dependencies

**ELF Components**:
```
┌─────────────────────┐
│ ELF Header          │ ← Magic number, arch, entry point
├─────────────────────┤
│ Program Headers     │ ← Loadable segments (LOAD, INTERP, DYNAMIC)
├─────────────────────┤
│ .text (code)        │
│ .data (initialized) │
│ .bss (zeroed)       │
│ .rodata (constants) │
├─────────────────────┤
│ Section Headers     │ ← Debugging info, symbols
└─────────────────────┘
```

### Stack Layout After exec

**Initial User Stack** (grows downward from high address):
```
High Address
┌──────────────────────┐
│ NULL                 │ ← envp terminator
│ env[n]               │
│ ...                  │
│ env[1]               │
│ env[0]               │
├──────────────────────┤
│ NULL                 │ ← argv terminator
│ argv[argc-1]         │
│ ...                  │
│ argv[1]              │
│ argv[0]              │
├──────────────────────┤
│ envp pointers        │ ← Array of char*
├──────────────────────┤
│ argv pointers        │ ← Array of char*
├──────────────────────┤
│ argc                 │ ← Argument count
└──────────────────────┘ ← RSP points here
Low Address
```

**Register State**:
- `RSP`: Points to argc
- `RIP`: Program entry point
- `RFLAGS`: 0x202 (interrupts enabled)
- All other registers: Zero

---

## Implementation Plan

### Phase 1.1: ELF Parsing Infrastructure (2-3 hours)

#### File: `src/kernel/elf_loader.hpp` (150 LOC)

**Purpose**: Define ELF structures and parsing interface

```cpp
/**
 * @file elf_loader.hpp
 * @brief ELF binary parsing and loading
 *
 * Supports ELF64 on x86_64.
 * Week 10 Phase 1: Static binaries only
 * Week 11: Dynamic linking support
 */

#pragma once
#include <cstdint>

namespace xinim::kernel {

// ============================================================================
// ELF64 Header
// ============================================================================

#define EI_NIDENT 16

struct Elf64_Ehdr {
    uint8_t  e_ident[EI_NIDENT];  // Magic number, class, endian, version
    uint16_t e_type;               // Object file type (ET_EXEC, ET_DYN)
    uint16_t e_machine;            // Architecture (EM_X86_64)
    uint32_t e_version;            // ELF version
    uint64_t e_entry;              // Entry point address
    uint64_t e_phoff;              // Program header offset
    uint64_t e_shoff;              // Section header offset
    uint32_t e_flags;              // Processor-specific flags
    uint16_t e_ehsize;             // ELF header size
    uint16_t e_phentsize;          // Program header entry size
    uint16_t e_phnum;              // Number of program headers
    uint16_t e_shentsize;          // Section header entry size
    uint16_t e_shnum;              // Number of section headers
    uint16_t e_shstrndx;           // String table index
};

// ============================================================================
// ELF64 Program Header
// ============================================================================

struct Elf64_Phdr {
    uint32_t p_type;               // Segment type (PT_LOAD, PT_INTERP, PT_DYNAMIC)
    uint32_t p_flags;              // Segment flags (PF_R, PF_W, PF_X)
    uint64_t p_offset;             // File offset
    uint64_t p_vaddr;              // Virtual address
    uint64_t p_paddr;              // Physical address (unused)
    uint64_t p_filesz;             // Size in file
    uint64_t p_memsz;              // Size in memory (>= filesz for .bss)
    uint64_t p_align;              // Alignment
};

// ============================================================================
// ELF Constants
// ============================================================================

// Magic number
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

// Class
#define ELFCLASS32 1
#define ELFCLASS64 2

// Type
#define ET_EXEC 2    // Executable file
#define ET_DYN  3    // Shared object file

// Machine
#define EM_X86_64 62

// Segment types
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_PHDR    6

// Segment flags
#define PF_X 0x1     // Execute
#define PF_W 0x2     // Write
#define PF_R 0x4     // Read

// ============================================================================
// ELF Loader Interface
// ============================================================================

/**
 * @brief ELF loading result
 */
struct ElfLoadInfo {
    uint64_t entry_point;      // Program entry point
    uint64_t stack_top;        // Top of user stack
    uint64_t brk_start;        // Start of heap (for brk syscall)
    bool has_interpreter;      // True if dynamic linking required
    char interpreter[256];     // Path to dynamic linker (/lib64/ld-linux.so)
};

/**
 * @brief Load ELF binary from file
 *
 * Steps:
 * 1. Read and validate ELF header
 * 2. Parse program headers
 * 3. Load PT_LOAD segments into memory
 * 4. Zero-initialize .bss sections
 * 5. Set up initial stack
 * 6. Return entry point and stack pointer
 *
 * @param pathname Path to ELF binary
 * @param load_info Output: loading information
 * @return 0 on success, negative errno on failure
 *         -ENOEXEC: Not a valid ELF binary
 *         -EINVAL: Unsupported architecture or format
 *         -ENOMEM: Out of memory
 */
int load_elf_binary(const char* pathname, ElfLoadInfo* load_info);

/**
 * @brief Validate ELF header
 *
 * Checks:
 * - Magic number (0x7f 'E' 'L' 'F')
 * - Class (ELF64)
 * - Machine (x86_64)
 * - Type (ET_EXEC or ET_DYN)
 */
bool validate_elf_header(const Elf64_Ehdr* ehdr);

/**
 * @brief Load PT_LOAD segment
 *
 * Steps:
 * 1. Allocate virtual memory at p_vaddr
 * 2. Read p_filesz bytes from file at p_offset
 * 3. Zero-initialize remaining p_memsz - p_filesz bytes (for .bss)
 * 4. Set memory protection based on p_flags
 */
int load_segment(int fd, const Elf64_Phdr* phdr);

} // namespace xinim::kernel
```

#### File: `src/kernel/elf_loader.cpp` (400 LOC)

**Purpose**: Implement ELF parsing and loading

```cpp
/**
 * @file elf_loader.cpp
 * @brief ELF binary parsing and loading implementation
 */

#include "elf_loader.hpp"
#include "vfs_interface.hpp"
#include "memory.hpp"
#include "uaccess.hpp"
#include <cerrno>
#include <cstring>
#include <cstdio>

namespace xinim::kernel {

/**
 * @brief Validate ELF header
 */
bool validate_elf_header(const Elf64_Ehdr* ehdr) {
    // Check magic number
    if (ehdr->e_ident[0] != ELFMAG0 ||
        ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 ||
        ehdr->e_ident[3] != ELFMAG3) {
        return false;
    }

    // Check class (must be ELF64)
    if (ehdr->e_ident[4] != ELFCLASS64) {
        return false;
    }

    // Check machine (must be x86_64)
    if (ehdr->e_machine != EM_X86_64) {
        return false;
    }

    // Check type (must be executable or shared object)
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        return false;
    }

    return true;
}

/**
 * @brief Load PT_LOAD segment
 */
int load_segment(int fd, const Elf64_Phdr* phdr) {
    // Week 10 Phase 1: Simple implementation
    // Week 11: Enhanced with demand paging

    // Allocate virtual memory
    void* vaddr = (void*)phdr->p_vaddr;
    size_t size = phdr->p_memsz;

    // Allocate pages (aligned to 4KB)
    size_t aligned_size = (size + 0xFFF) & ~0xFFF;
    void* mem = allocate_user_memory(aligned_size);
    if (!mem) {
        return -ENOMEM;
    }

    // Read file content
    if (phdr->p_filesz > 0) {
        ssize_t bytes_read = vfs_read_at(fd, mem, phdr->p_filesz, phdr->p_offset);
        if (bytes_read != (ssize_t)phdr->p_filesz) {
            free_user_memory(mem, aligned_size);
            return -EIO;
        }
    }

    // Zero-initialize BSS section
    if (phdr->p_memsz > phdr->p_filesz) {
        size_t bss_size = phdr->p_memsz - phdr->p_filesz;
        memset((char*)mem + phdr->p_filesz, 0, bss_size);
    }

    // Map into user address space
    uint32_t prot = 0;
    if (phdr->p_flags & PF_R) prot |= PROT_READ;
    if (phdr->p_flags & PF_W) prot |= PROT_WRITE;
    if (phdr->p_flags & PF_X) prot |= PROT_EXEC;

    int ret = map_user_memory(vaddr, mem, aligned_size, prot);
    if (ret < 0) {
        free_user_memory(mem, aligned_size);
        return ret;
    }

    return 0;
}

/**
 * @brief Load ELF binary from file
 */
int load_elf_binary(const char* pathname, ElfLoadInfo* load_info) {
    // Open executable file
    void* inode = vfs_lookup(pathname);
    if (!inode) {
        return -ENOENT;
    }

    int fd = vfs_open(inode, O_RDONLY);
    if (fd < 0) {
        return fd;
    }

    // Read ELF header
    Elf64_Ehdr ehdr;
    ssize_t bytes_read = vfs_read(inode, &ehdr, sizeof(ehdr), 0);
    if (bytes_read != sizeof(ehdr)) {
        vfs_close(fd);
        return -ENOEXEC;
    }

    // Validate header
    if (!validate_elf_header(&ehdr)) {
        vfs_close(fd);
        return -ENOEXEC;
    }

    // Parse program headers
    size_t phdr_size = ehdr.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr* phdrs = (Elf64_Phdr*)malloc(phdr_size);
    if (!phdrs) {
        vfs_close(fd);
        return -ENOMEM;
    }

    bytes_read = vfs_read_at(fd, phdrs, phdr_size, ehdr.e_phoff);
    if (bytes_read != (ssize_t)phdr_size) {
        free(phdrs);
        vfs_close(fd);
        return -ENOEXEC;
    }

    // Load segments
    uint64_t brk_start = 0;
    bool has_interpreter = false;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr* phdr = &phdrs[i];

        switch (phdr->p_type) {
            case PT_LOAD:
                // Load segment into memory
                int ret = load_segment(fd, phdr);
                if (ret < 0) {
                    free(phdrs);
                    vfs_close(fd);
                    return ret;
                }

                // Track end of last LOAD segment for brk
                uint64_t seg_end = phdr->p_vaddr + phdr->p_memsz;
                if (seg_end > brk_start) {
                    brk_start = seg_end;
                }
                break;

            case PT_INTERP:
                // Dynamic linker requested
                has_interpreter = true;
                vfs_read_at(fd, load_info->interpreter, phdr->p_filesz, phdr->p_offset);
                break;

            case PT_DYNAMIC:
                // Dynamic linking info (Week 11)
                break;

            default:
                // Ignore other segment types
                break;
        }
    }

    // Set up return info
    load_info->entry_point = ehdr.e_entry;
    load_info->stack_top = USER_STACK_TOP;  // 0x00007FFFFFFFFFFF
    load_info->brk_start = (brk_start + 0xFFF) & ~0xFFF;  // Page-aligned
    load_info->has_interpreter = has_interpreter;

    free(phdrs);
    vfs_close(fd);

    return 0;
}

} // namespace xinim::kernel
```

---

### Phase 1.2: exec Stack Setup (2-3 hours)

#### File: `src/kernel/exec_stack.hpp` (80 LOC)

```cpp
/**
 * @file exec_stack.hpp
 * @brief Initial user stack setup for execve
 */

#pragma once
#include <cstdint>

namespace xinim::kernel {

/**
 * @brief Set up initial user stack
 *
 * Creates stack layout:
 * - argc
 * - argv pointers
 * - envp pointers
 * - NULL terminators
 * - String data
 *
 * @param stack_top Top of user stack (high address)
 * @param argv Argument vector (NULL-terminated)
 * @param envp Environment vector (NULL-terminated)
 * @return New stack pointer (RSP value)
 */
uint64_t setup_exec_stack(uint64_t stack_top,
                          char* const argv[],
                          char* const envp[]);

/**
 * @brief Count NULL-terminated pointer array
 */
int count_strings(char* const strings[]);

/**
 * @brief Calculate total size of string data
 */
size_t calculate_string_size(char* const strings[]);

} // namespace xinim::kernel
```

#### File: `src/kernel/exec_stack.cpp` (250 LOC)

```cpp
/**
 * @file exec_stack.cpp
 * @brief Initial user stack setup implementation
 */

#include "exec_stack.hpp"
#include <cstring>
#include <cstdio>

namespace xinim::kernel {

/**
 * @brief Count NULL-terminated pointer array
 */
int count_strings(char* const strings[]) {
    if (!strings) return 0;

    int count = 0;
    while (strings[count]) {
        count++;
    }
    return count;
}

/**
 * @brief Calculate total size of string data
 */
size_t calculate_string_size(char* const strings[]) {
    if (!strings) return 0;

    size_t total = 0;
    for (int i = 0; strings[i]; i++) {
        total += strlen(strings[i]) + 1;  // +1 for NULL terminator
    }
    return total;
}

/**
 * @brief Set up initial user stack
 */
uint64_t setup_exec_stack(uint64_t stack_top,
                          char* const argv[],
                          char* const envp[]) {
    // Count arguments and environment variables
    int argc = count_strings(argv);
    int envc = count_strings(envp);

    // Calculate sizes
    size_t argv_str_size = calculate_string_size(argv);
    size_t envp_str_size = calculate_string_size(envp);

    // Calculate total stack size
    size_t total_size = 0;
    total_size += sizeof(uint64_t);              // argc
    total_size += (argc + 1) * sizeof(char*);    // argv pointers + NULL
    total_size += (envc + 1) * sizeof(char*);    // envp pointers + NULL
    total_size += argv_str_size;                 // argv strings
    total_size += envp_str_size;                 // envp strings

    // Align to 16 bytes (required by x86_64 ABI)
    total_size = (total_size + 15) & ~15;

    // Stack grows downward
    uint64_t sp = stack_top - total_size;
    uint64_t string_area = sp + sizeof(uint64_t) +
                           (argc + 1) * sizeof(char*) +
                           (envc + 1) * sizeof(char*);

    // Write argc
    *(uint64_t*)sp = (uint64_t)argc;
    sp += sizeof(uint64_t);

    // Write argv pointers
    uint64_t current_string = string_area;
    for (int i = 0; i < argc; i++) {
        *(char**)sp = (char*)current_string;
        sp += sizeof(char*);

        // Copy string
        size_t len = strlen(argv[i]) + 1;
        memcpy((void*)current_string, argv[i], len);
        current_string += len;
    }

    // NULL terminator for argv
    *(char**)sp = nullptr;
    sp += sizeof(char*);

    // Write envp pointers
    for (int i = 0; i < envc; i++) {
        *(char**)sp = (char*)current_string;
        sp += sizeof(char*);

        // Copy string
        size_t len = strlen(envp[i]) + 1;
        memcpy((void*)current_string, envp[i], len);
        current_string += len;
    }

    // NULL terminator for envp
    *(char**)sp = nullptr;

    // Return stack pointer (points to argc)
    return stack_top - total_size;
}

} // namespace xinim::kernel
```

---

### Phase 1.3: sys_execve Implementation (3-4 hours)

#### File: `src/kernel/syscalls/exec.cpp` (400 LOC)

```cpp
/**
 * @file exec.cpp
 * @brief execve syscall implementation
 */

#include "../syscall_table.hpp"
#include "../elf_loader.hpp"
#include "../exec_stack.hpp"
#include "../uaccess.hpp"
#include "../pcb.hpp"
#include "../fd_table.hpp"
#include "../scheduler.hpp"
#include "../../early/serial_16550.hpp"
#include <cerrno>
#include <cstring>
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

/**
 * @brief Copy string array from user space
 *
 * Copies NULL-terminated array of strings (argv or envp).
 *
 * @param kernel_array Output: kernel buffer for pointers
 * @param user_array_addr User-space array address
 * @param max_count Maximum number of strings
 * @return Number of strings copied, or negative error
 */
static int copy_string_array(char* kernel_array[],
                             uint64_t user_array_addr,
                             int max_count) {
    if (!is_user_address(user_array_addr, sizeof(char*))) {
        return -EFAULT;
    }

    int count = 0;
    char** user_array = (char**)user_array_addr;

    while (count < max_count) {
        // Read pointer from user space
        char* user_str;
        int ret = copy_from_user(&user_str, (uint64_t)&user_array[count], sizeof(char*));
        if (ret < 0) return ret;

        // NULL terminator
        if (user_str == nullptr) {
            kernel_array[count] = nullptr;
            break;
        }

        // Copy string from user space
        char* kernel_str = (char*)malloc(PATH_MAX);
        if (!kernel_str) {
            // Free previously allocated strings
            for (int i = 0; i < count; i++) {
                free(kernel_array[i]);
            }
            return -ENOMEM;
        }

        ret = copy_string_from_user(kernel_str, (uint64_t)user_str, PATH_MAX);
        if (ret < 0) {
            free(kernel_str);
            for (int i = 0; i < count; i++) {
                free(kernel_array[i]);
            }
            return ret;
        }

        kernel_array[count] = kernel_str;
        count++;
    }

    return count;
}

/**
 * @brief Free string array
 */
static void free_string_array(char* array[]) {
    for (int i = 0; array[i]; i++) {
        free(array[i]);
    }
}

/**
 * @brief Close file descriptors with CLOEXEC flag
 */
static void close_cloexec_fds(FileDescriptorTable* fd_table) {
    for (int fd = 0; fd < MAX_FDS_PER_PROCESS; fd++) {
        FileDescriptor* fd_entry = fd_table->get_fd(fd);
        if (fd_entry && fd_entry->is_open) {
            if (fd_entry->flags & (uint32_t)FdFlags::CLOEXEC) {
                fd_table->close_fd(fd);
            }
        }
    }
}

/**
 * @brief sys_execve - Execute new program
 *
 * POSIX: int execve(const char *pathname,
 *                   char *const argv[],
 *                   char *const envp[])
 *
 * @param pathname_addr User-space pointer to executable path
 * @param argv_addr User-space pointer to argument array
 * @param envp_addr User-space pointer to environment array
 * @return Does not return on success, negative error on failure
 */
extern "C" [[noreturn]] int64_t sys_execve(uint64_t pathname_addr,
                                            uint64_t argv_addr,
                                            uint64_t envp_addr,
                                            uint64_t, uint64_t, uint64_t) {
    ProcessControlBlock* current = get_current_process();
    if (!current) {
        // Should never happen
        for(;;) { __asm__ volatile("hlt"); }
    }

    // Copy pathname from user space
    char pathname[PATH_MAX];
    int ret = copy_string_from_user(pathname, pathname_addr, PATH_MAX);
    if (ret < 0) {
        current->context.rax = (uint64_t)ret;
        schedule();  // Return to user mode with error
        __builtin_unreachable();
    }

    // Copy argv from user space
    char* argv[256];  // Max 256 arguments
    int argc = copy_string_array(argv, argv_addr, 256);
    if (argc < 0) {
        current->context.rax = (uint64_t)argc;
        schedule();
        __builtin_unreachable();
    }

    // Copy envp from user space
    char* envp[256];  // Max 256 environment variables
    int envc = copy_string_array(envp, envp_addr, 256);
    if (envc < 0) {
        free_string_array(argv);
        current->context.rax = (uint64_t)envc;
        schedule();
        __builtin_unreachable();
    }

    // Log execve
    char log_buf[256];
    snprintf(log_buf, sizeof(log_buf),
             "[SYSCALL] Process %d execve(\"%s\", argc=%d)\n",
             current->pid, pathname, argc);
    early_serial.write(log_buf);

    // Load ELF binary
    ElfLoadInfo load_info;
    ret = load_elf_binary(pathname, &load_info);
    if (ret < 0) {
        free_string_array(argv);
        free_string_array(envp);
        current->context.rax = (uint64_t)ret;
        schedule();
        __builtin_unreachable();
    }

    // Check for dynamic linking (Week 11)
    if (load_info.has_interpreter) {
        snprintf(log_buf, sizeof(log_buf),
                 "[EXECVE] Dynamic linking not yet supported (Week 11)\n");
        early_serial.write(log_buf);
        free_string_array(argv);
        free_string_array(envp);
        current->context.rax = (uint64_t)-ENOEXEC;
        schedule();
        __builtin_unreachable();
    }

    // Close FDs with CLOEXEC flag
    close_cloexec_fds(&current->fd_table);

    // Set up new stack
    uint64_t new_sp = setup_exec_stack(load_info.stack_top, argv, envp);

    // Free argv/envp (already copied to user stack)
    free_string_array(argv);
    free_string_array(envp);

    // Update process name
    strncpy(current->name, pathname, sizeof(current->name) - 1);

    // Update PCB state
    current->brk = load_info.brk_start;

    // Reset signal handlers to default (Week 10 Phase 2)
    // reset_signal_handlers(current);

    // Set up CPU context for new program
    current->context.rip = load_info.entry_point;
    current->context.rsp = new_sp;
    current->context.rbp = 0;
    current->context.rflags = 0x202;  // Interrupts enabled

    // Clear general-purpose registers
    current->context.rax = 0;
    current->context.rbx = 0;
    current->context.rcx = 0;
    current->context.rdx = 0;
    current->context.rsi = 0;
    current->context.rdi = 0;
    current->context.r8  = 0;
    current->context.r9  = 0;
    current->context.r10 = 0;
    current->context.r11 = 0;
    current->context.r12 = 0;
    current->context.r13 = 0;
    current->context.r14 = 0;
    current->context.r15 = 0;

    snprintf(log_buf, sizeof(log_buf),
             "[EXECVE] Process %d jumping to entry point 0x%lx\n",
             current->pid, load_info.entry_point);
    early_serial.write(log_buf);

    // Never returns - jump to new program
    schedule();
    __builtin_unreachable();
}

} // namespace xinim::kernel
```

---

## Implementation Steps (Granular TODO)

### Step 1: ELF Infrastructure (2-3 hours)
1. ✅ Create `src/kernel/elf_loader.hpp` (150 LOC)
2. ✅ Create `src/kernel/elf_loader.cpp` (400 LOC)
3. ✅ Add memory allocation stubs if needed
4. ✅ Test ELF header validation with sample binary

### Step 2: Stack Setup (2-3 hours)
1. ✅ Create `src/kernel/exec_stack.hpp` (80 LOC)
2. ✅ Create `src/kernel/exec_stack.cpp` (250 LOC)
3. ✅ Unit test stack layout calculation
4. ✅ Verify alignment requirements

### Step 3: sys_execve (3-4 hours)
1. ✅ Create `src/kernel/syscalls/exec.cpp` (400 LOC)
2. ✅ Implement string array copying
3. ✅ Implement CLOEXEC handling
4. ✅ Integrate ELF loader and stack setup
5. ✅ Update syscall table (syscall 59)
6. ✅ Update build system

### Step 4: Testing (1-2 hours)
1. ✅ Create simple test binary (`userland/test_programs/test_hello.c`)
2. ✅ Test execve from kernel init process
3. ✅ Verify argc/argv/envp
4. ✅ Test CLOEXEC behavior
5. ✅ Test failure cases (invalid path, bad ELF)

---

## Files Created

| File | LOC | Purpose |
|------|-----|---------|
| `src/kernel/elf_loader.hpp` | 150 | ELF structures and interface |
| `src/kernel/elf_loader.cpp` | 400 | ELF parsing and loading |
| `src/kernel/exec_stack.hpp` | 80 | Stack setup interface |
| `src/kernel/exec_stack.cpp` | 250 | Stack initialization |
| `src/kernel/syscalls/exec.cpp` | 400 | sys_execve implementation |
| `userland/test_programs/test_hello.c` | 50 | Test binary |
| `docs/WEEK10_PHASE1_EXECVE_PLAN.md` | 500 | This document |
| **Total** | **1830** | |

---

## Testing Strategy

### Unit Tests
```cpp
// Test 1: ELF header validation
TEST(ElfLoader, ValidateHeader) {
    Elf64_Ehdr valid_header = create_valid_header();
    EXPECT_TRUE(validate_elf_header(&valid_header));

    Elf64_Ehdr invalid_magic = valid_header;
    invalid_magic.e_ident[0] = 0xFF;
    EXPECT_FALSE(validate_elf_header(&invalid_magic));
}

// Test 2: Stack setup
TEST(ExecStack, SetupStack) {
    char* argv[] = {"test", "arg1", "arg2", nullptr};
    char* envp[] = {"VAR=value", nullptr};

    uint64_t sp = setup_exec_stack(0x7FFFFFFFFFFF, argv, envp);

    // Verify alignment
    EXPECT_EQ(sp & 15, 0);

    // Verify argc
    EXPECT_EQ(*(uint64_t*)sp, 3);
}

// Test 3: String array copying
TEST(ExecSyscall, CopyStringArray) {
    // Test valid array
    // Test NULL termination
    // Test EFAULT on invalid pointer
    // Test ENOMEM on too many strings
}
```

### Integration Tests
```cpp
// Test 4: Simple execve
TEST(Week10, SimpleExecve) {
    if (fork() == 0) {
        char* argv[] = {"test_hello", nullptr};
        char* envp[] = {nullptr};
        execve("/bin/test_hello", argv, envp);
        // Should not reach here
        exit(1);
    }

    int status;
    wait4(-1, &status, 0, nullptr);
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

// Test 5: argc/argv
TEST(Week10, ExecveArgs) {
    if (fork() == 0) {
        char* argv[] = {"test_args", "arg1", "arg2", "arg3", nullptr};
        char* envp[] = {nullptr};
        execve("/bin/test_args", argv, envp);
        exit(1);
    }

    int status;
    wait4(-1, &status, 0, nullptr);
    // test_args verifies it received 4 arguments
}

// Test 6: Environment variables
TEST(Week10, ExecveEnv) {
    if (fork() == 0) {
        char* argv[] = {"test_env", nullptr};
        char* envp[] = {"VAR1=value1", "VAR2=value2", nullptr};
        execve("/bin/test_env", argv, envp);
        exit(1);
    }

    int status;
    wait4(-1, &status, 0, nullptr);
    // test_env verifies environment variables
}

// Test 7: CLOEXEC
TEST(Week10, ExecveCLOEXEC) {
    int fd = open("/dev/console", O_RDWR | O_CLOEXEC);

    if (fork() == 0) {
        char* argv[] = {"test_fd", nullptr};
        char* envp[] = {nullptr};
        execve("/bin/test_fd", argv, envp);
        exit(1);
    }

    close(fd);

    int status;
    wait4(-1, &status, 0, nullptr);
    // test_fd verifies FD was closed
}
```

---

## Success Criteria

Week 10 Phase 1 is complete when:
- ✅ ELF binaries load and parse correctly
- ✅ Static executables run successfully
- ✅ argc/argv/envp passed correctly to new program
- ✅ CLOEXEC file descriptors closed on exec
- ✅ Process identity (PID, PPID, FDs) preserved
- ✅ All unit tests pass
- ✅ All integration tests pass
- ✅ Code committed and pushed

---

## Next Steps

After Week 10 Phase 1:
1. **Week 10 Phase 2**: Implement signal handling
2. **Week 10 Phase 3**: Create test shell and integration tests
3. **Week 11**: Memory mapping and shared memory
4. **Week 12**: Security, optimization, and comprehensive testing

---

**Week 10 Phase 1 Plan Complete**
**Ready to implement execve step-by-step**

Estimated Effort: 8-10 hours
Total LOC: ~1830
