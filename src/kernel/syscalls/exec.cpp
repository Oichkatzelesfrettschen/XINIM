/**
 * @file exec.cpp
 * @brief execve syscall implementation
 *
 * Implements the execve system call for program execution.
 * Replaces current process image with new program.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
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

// Maximum arguments and environment variables
constexpr int MAX_ARG_STRINGS = 256;
constexpr size_t MAX_ARG_STRLEN = 4096;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Copy string array from user space
 *
 * Copies NULL-terminated array of strings (argv or envp) from user space
 * to kernel space.
 *
 * @param kernel_array Output: kernel buffer for string pointers
 * @param user_array_addr User-space array address
 * @param max_count Maximum number of strings
 * @return Number of strings copied, or negative error
 */
static int copy_string_array(char* kernel_array[],
                             uint64_t user_array_addr,
                             int max_count) {
    if (user_array_addr == 0) {
        // NULL array is allowed (e.g., empty envp)
        kernel_array[0] = nullptr;
        return 0;
    }

    if (!is_user_address(user_array_addr, sizeof(char*))) {
        early_serial.write("[EXECVE] Invalid user array pointer\n");
        return -EFAULT;
    }

    int count = 0;

    while (count < max_count) {
        // Read pointer from user space
        uint64_t user_str_ptr;
        int ret = copy_from_user(&user_str_ptr,
                                 user_array_addr + count * sizeof(char*),
                                 sizeof(char*));
        if (ret < 0) {
            // Free previously allocated strings
            for (int i = 0; i < count; i++) {
                delete[] kernel_array[i];
            }
            return ret;
        }

        // NULL terminator found
        if (user_str_ptr == 0) {
            kernel_array[count] = nullptr;
            break;
        }

        // Allocate kernel buffer for string
        char* kernel_str = new char[MAX_ARG_STRLEN];
        if (!kernel_str) {
            // Free previously allocated strings
            for (int i = 0; i < count; i++) {
                delete[] kernel_array[i];
            }
            early_serial.write("[EXECVE] Failed to allocate string buffer\n");
            return -ENOMEM;
        }

        // Copy string from user space
        ret = copy_string_from_user(kernel_str, user_str_ptr, MAX_ARG_STRLEN);
        if (ret < 0) {
            delete[] kernel_str;
            // Free previously allocated strings
            for (int i = 0; i < count; i++) {
                delete[] kernel_array[i];
            }
            return ret;
        }

        kernel_array[count] = kernel_str;
        count++;
    }

    if (count >= max_count) {
        // Too many strings
        for (int i = 0; i < count; i++) {
            delete[] kernel_array[i];
        }
        early_serial.write("[EXECVE] Too many argument/environment strings\n");
        return -E2BIG;
    }

    return count;
}

/**
 * @brief Free string array
 */
static void free_string_array(char* array[]) {
    for (int i = 0; array[i] != nullptr; i++) {
        delete[] array[i];
    }
}

/**
 * @brief Close file descriptors with CLOEXEC flag
 *
 * Called during execve to close FDs marked as close-on-exec.
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

// ============================================================================
// sys_execve - Execute new program
// ============================================================================

/**
 * @brief sys_execve - Execute new program
 *
 * POSIX: int execve(const char *pathname,
 *                   char *const argv[],
 *                   char *const envp[])
 *
 * Process:
 * 1. Copy pathname, argv, envp from user space
 * 2. Load ELF binary
 * 3. Free old address space (Week 11)
 * 4. Close FDs with CLOEXEC
 * 5. Set up new stack
 * 6. Reset signal handlers (Week 10 Phase 2)
 * 7. Jump to entry point (never returns on success)
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
        // Should never happen - no current process
        early_serial.write("[EXECVE] FATAL: No current process\n");
        for(;;) { __asm__ volatile("hlt"); }
    }

    char log_buf[256];

    // ========================================================================
    // Step 1: Copy arguments from user space
    // ========================================================================

    // Copy pathname
    char pathname[PATH_MAX];
    int ret = copy_string_from_user(pathname, pathname_addr, PATH_MAX);
    if (ret < 0) {
        std::snprintf(log_buf, sizeof(log_buf),
                      "[EXECVE] Failed to copy pathname: %d\n", ret);
        early_serial.write(log_buf);
        current->context.rax = (uint64_t)ret;
        schedule();
        __builtin_unreachable();
    }

    std::snprintf(log_buf, sizeof(log_buf),
                  "[EXECVE] Process %d execve(\"%s\")\n",
                  current->pid, pathname);
    early_serial.write(log_buf);

    // Copy argv
    char* argv[MAX_ARG_STRINGS + 1];
    int argc = copy_string_array(argv, argv_addr, MAX_ARG_STRINGS);
    if (argc < 0) {
        std::snprintf(log_buf, sizeof(log_buf),
                      "[EXECVE] Failed to copy argv: %d\n", argc);
        early_serial.write(log_buf);
        current->context.rax = (uint64_t)argc;
        schedule();
        __builtin_unreachable();
    }

    std::snprintf(log_buf, sizeof(log_buf),
                  "[EXECVE] argc = %d\n", argc);
    early_serial.write(log_buf);

    // Copy envp
    char* envp[MAX_ARG_STRINGS + 1];
    int envc = copy_string_array(envp, envp_addr, MAX_ARG_STRINGS);
    if (envc < 0) {
        free_string_array(argv);
        std::snprintf(log_buf, sizeof(log_buf),
                      "[EXECVE] Failed to copy envp: %d\n", envc);
        early_serial.write(log_buf);
        current->context.rax = (uint64_t)envc;
        schedule();
        __builtin_unreachable();
    }

    std::snprintf(log_buf, sizeof(log_buf),
                  "[EXECVE] envc = %d\n", envc);
    early_serial.write(log_buf);

    // ========================================================================
    // Step 2: Load ELF binary
    // ========================================================================

    ElfLoadInfo load_info;
    ret = load_elf_binary(pathname, &load_info);
    if (ret < 0) {
        free_string_array(argv);
        free_string_array(envp);
        std::snprintf(log_buf, sizeof(log_buf),
                      "[EXECVE] Failed to load binary: %d\n", ret);
        early_serial.write(log_buf);
        current->context.rax = (uint64_t)ret;
        schedule();
        __builtin_unreachable();
    }

    // Week 10 Phase 1: No dynamic linking support yet
    if (load_info.has_interpreter) {
        free_string_array(argv);
        free_string_array(envp);
        std::snprintf(log_buf, sizeof(log_buf),
                      "[EXECVE] Dynamic linking not supported (interpreter: %s)\n",
                      load_info.interpreter);
        early_serial.write(log_buf);
        std::snprintf(log_buf, sizeof(log_buf),
                      "[EXECVE] Week 11 will add dynamic linker support\n");
        early_serial.write(log_buf);
        current->context.rax = (uint64_t)-ENOEXEC;
        schedule();
        __builtin_unreachable();
    }

    // ========================================================================
    // Step 3: Free old address space
    // ========================================================================

    // TODO Week 10 Phase 1: Free old user memory pages
    // TODO Week 11: Implement proper VMA cleanup and page table updates

    early_serial.write("[EXECVE] Old address space cleanup (placeholder)\n");

    // ========================================================================
    // Step 4: Close CLOEXEC file descriptors
    // ========================================================================

    close_cloexec_fds(&current->fd_table);

    early_serial.write("[EXECVE] Closed CLOEXEC file descriptors\n");

    // ========================================================================
    // Step 5: Set up new stack
    // ========================================================================

    uint64_t new_sp = setup_exec_stack(load_info.stack_top, argv, envp);
    if (new_sp == 0) {
        free_string_array(argv);
        free_string_array(envp);
        early_serial.write("[EXECVE] Failed to setup stack\n");
        current->context.rax = (uint64_t)-ENOMEM;
        schedule();
        __builtin_unreachable();
    }

    // Free argv/envp kernel buffers (already copied to user stack)
    free_string_array(argv);
    free_string_array(envp);

    // ========================================================================
    // Step 6: Reset signal handlers
    // ========================================================================

    // TODO Week 10 Phase 2: Reset signal handlers to default
    // reset_signal_handlers(current);

    early_serial.write("[EXECVE] Signal handlers reset (placeholder)\n");

    // ========================================================================
    // Step 7: Update process state
    // ========================================================================

    // Update process name
    std::strncpy(current->name, pathname, sizeof(current->name) - 1);
    current->name[sizeof(current->name) - 1] = '\0';

    // Update heap break
    current->brk = load_info.brk_start;

    std::snprintf(log_buf, sizeof(log_buf),
                  "[EXECVE] Process %d: name=\"%s\" brk=0x%lx\n",
                  current->pid, current->name, current->brk);
    early_serial.write(log_buf);

    // ========================================================================
    // Step 8: Set up CPU context for new program
    // ========================================================================

    // Entry point
    current->context.rip = load_info.entry_point;

    // Stack pointer (points to argc)
    current->context.rsp = new_sp;
    current->context.rbp = 0;

    // Flags (interrupts enabled)
    current->context.rflags = 0x202;

    // Clear all general-purpose registers
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

    // Segment selectors (user mode)
    current->context.cs = 0x23;  // User code segment (RPL=3)
    current->context.ss = 0x1B;  // User data segment (RPL=3)
    current->context.ds = 0x1B;
    current->context.es = 0x1B;
    current->context.fs = 0;
    current->context.gs = 0;

    std::snprintf(log_buf, sizeof(log_buf),
                  "[EXECVE] Jumping to entry point: rip=0x%lx rsp=0x%lx\n",
                  current->context.rip, current->context.rsp);
    early_serial.write(log_buf);

    // ========================================================================
    // Step 9: Jump to new program (never returns)
    // ========================================================================

    early_serial.write("[EXECVE] Execution transfer successful\n");

    // Return to userspace via scheduler
    // The new program will start executing from load_info.entry_point
    schedule();

    // Should never reach here
    __builtin_unreachable();
}

} // namespace xinim::kernel
