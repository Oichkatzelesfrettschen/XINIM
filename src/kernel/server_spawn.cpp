/**
 * @file server_spawn.cpp
 * @brief Userspace server spawn infrastructure for XINIM microkernel
 *
 * This module implements the kernel-side boot infrastructure for spawning
 * the three core userspace servers (VFS, Process Manager, Memory Manager)
 * during system initialization.
 *
 * Boot Sequence:
 * 1. Kernel early initialization (hardware, memory, IPC)
 * 2. initialize_system_servers() spawns:
 *    - VFS Server (PID 2)
 *    - Process Manager (PID 3)
 *    - Memory Manager (PID 4)
 * 3. spawn_init_process() creates init (PID 1)
 * 4. Kernel enters scheduler loop
 *
 * Boot-time servers receive Ring 3 CPU contexts, private kernel stacks, and
 * well-known process identifiers before the scheduler starts.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 */

#include "server_spawn.hpp"

#include "context.hpp"
#include "early/serial_16550.hpp"
#include "fd_table.hpp"
#include "lattice_ipc.hpp"
#include "pcb.hpp"
#include "scheduler.hpp"
#include "signal.hpp"
#include "unified_scheduler.hpp"
#include "vfs_interface.hpp"
#include "x86_64/staged_xash.hpp"
#ifdef XINIM_ARCH_X86_64
#include "arch/x86_64/bootfs_syscalls.hpp"
#include "arch/x86_64/elf64_user_image.hpp"
#endif
#include "../include/xinim/ipc/message_types.h"

#include <cstdint>
#include <cstdio> // For snprintf
#include <cstring>

// External references
extern xinim::early::Serial16550 early_serial;

// Forward declarations for server entry points
extern "C" void vfs_server_main();
extern "C" void proc_mgr_main();
extern "C" void mem_mgr_main();

namespace xinim::kernel {

    // Server allocations route through the unified free-list heap in klib64.cpp.

    extern "C" void *malloc(size_t);
    extern "C" void free(void *);

    static void *kmalloc(uint64_t size) {
        void *ptr = malloc(static_cast<size_t>(size));
        if (ptr) {
            memset(ptr, 0, static_cast<size_t>(size));
        } else {
            early_serial.write("[ERROR] kernel heap exhausted\n");
        }
        return ptr;
    }

    // ============================================================================
    // Process Identifier Allocation
    // ============================================================================

    namespace {
        uint64_t g_next_pid = 1;
    }

    /**
     * @brief Allocate a new PID
     */
    xinim::pid_t allocate_process_id() noexcept {
        constexpr uint64_t kFirstUserPid = 1U;
        for (int attempt = 0; attempt < MAX_PROCESSES - 1; ++attempt) {
            if (g_next_pid < kFirstUserPid || g_next_pid >= static_cast<uint64_t>(MAX_PROCESSES)) {
                g_next_pid = kFirstUserPid;
            }
            const auto candidate = static_cast<xinim::pid_t>(g_next_pid++);
            if (find_process_by_pid(candidate) == nullptr) {
                return candidate;
            }
        }
        return -1;
    }

    /**
     * @brief Create a new PCB with a specific PID
     *
     * Used for well-known server PIDs (2, 3, 4).
     */
    ProcessControlBlock *create_process_control_block(xinim::pid_t pid) noexcept {
        auto *pcb = static_cast<ProcessControlBlock *>(kmalloc(sizeof(ProcessControlBlock)));

        if (!pcb) {
            return nullptr;
        }

        pcb->pid = pid;
        pcb->name = pcb->name_storage.data();
        pcb->name_storage[0] = '\0';
        pcb->state = ProcessState::CREATED;
        pcb->priority = 10; // Default priority
        pcb->stack_base = nullptr;
        pcb->stack_size = 0;
        pcb->next = nullptr;
        pcb->exit_status = 0;
        pcb->termination_signal = 0;
        pcb->stop_signal = 0;

        pcb->parent_pid = 0; // No parent initially (set by spawn_server or fork)
        pcb->children_head = nullptr;
        pcb->has_exited = false;
        pcb->has_been_waited = false;
        pcb->has_execed = false;
        pcb->wait_stopped_pending = false;
        pcb->wait_continued_pending = false;
        pcb->wait_status_address = 0U;
        pcb->wait_target_pid = -1;
        pcb->wait_options = 0;
        pcb->brk = 0U;
        pcb->minimum_break = 0U;
        pcb->user_mappings.reset();
        pcb->wake_deadline_tick = 0U;
        pcb->sleep_remaining_address = 0U;
        pcb->select_deadline_tick = 0U;
        pcb->select_timeout_address = 0U;
        pcb->select_active = false;
        pcb->alarm_deadline_tick = 0U;
        pcb->total_ticks = 0U;
        pcb->children_ticks = 0U;
        pcb->descriptor_limit = MAX_FDS_PER_PROCESS;
        pcb->file_creation_mask = 0022U;
        pcb->nice_value = 0;
        pcb->real_user_id = 0U;
        pcb->effective_user_id = 0U;
        pcb->saved_user_id = 0U;
        pcb->real_group_id = 0U;
        pcb->effective_group_id = 0U;
        pcb->saved_group_id = 0U;
        pcb->supplementary_groups.fill(0U);
        pcb->supplementary_group_count = 0U;

        pcb->pgid = pid; // Initially, process is its own group leader
        pcb->sid = pid;  // Initially, process is its own session leader
        pcb->pg_next = nullptr;
        pcb->pg_prev = nullptr;

        // Zero initialize context
        memset(&pcb->context, 0, sizeof(CpuContext));

        pcb->fd_table.initialize();
#ifdef XINIM_ARCH_X86_64
        xinim::kernel::x86_64::bootfs_initialize_process(*pcb);
#endif

        // Standard input, output, and error all point to /dev/console.
        void *console_inode = vfs_lookup("/dev/console");
        if (console_inode) {
            // FD 0: stdin (read-only)
            int fd0 = pcb->fd_table.allocate_fd();
            if (fd0 == 0) {
                FileDescriptor *stdin_fd = pcb->fd_table.get_fd(0);
                stdin_fd->is_open = true;
                stdin_fd->flags = 0;
                stdin_fd->file_flags = (uint32_t) FileFlags::RDONLY;
                stdin_fd->offset = 0;
                stdin_fd->inode = console_inode;
                stdin_fd->private_data = nullptr;
            }

            // FD 1: stdout (write-only)
            int fd1 = pcb->fd_table.allocate_fd();
            if (fd1 == 1) {
                FileDescriptor *stdout_fd = pcb->fd_table.get_fd(1);
                stdout_fd->is_open = true;
                stdout_fd->flags = 0;
                stdout_fd->file_flags = (uint32_t) FileFlags::WRONLY;
                stdout_fd->offset = 0;
                stdout_fd->inode = console_inode;
                stdout_fd->private_data = nullptr;
            }

            // FD 2: stderr (write-only)
            int fd2 = pcb->fd_table.allocate_fd();
            if (fd2 == 2) {
                FileDescriptor *stderr_fd = pcb->fd_table.get_fd(2);
                stderr_fd->is_open = true;
                stderr_fd->flags = 0;
                stderr_fd->file_flags = (uint32_t) FileFlags::WRONLY;
                stderr_fd->offset = 0;
                stderr_fd->inode = console_inode;
                stderr_fd->private_data = nullptr;
            }
        }

        if (!init_signal_state(pcb)) {
            free(pcb);
            return nullptr;
        }

        // Update g_next_pid if necessary
        if (static_cast<uint64_t>(pid) >= g_next_pid) {
            g_next_pid = static_cast<uint64_t>(pid) + 1;
        }

        return pcb;
    }

    // ============================================================================
    // Lattice IPC Registration
    // ============================================================================

    /**
     * @brief Register server with Lattice IPC system
     *
     * This registers the server's PID with the IPC subsystem so that
     * other processes can send messages to it.
     */
    static void lattice_register_server(xinim::pid_t pid, const char *name) {
        lattice::lattice_listen(pid);

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "[IPC] Registered server '%s' with PID %d\n", name, pid);
        early_serial.write(buffer);
    }

    // ============================================================================
    // Server Spawning Implementation
    // ============================================================================

    /**
     * @brief Spawn a userspace server with a well-known PID
     *
     * Steps:
     * 1. Allocate stack memory
     * 2. Create PCB with well-known PID
     * 3. Set up initial CPU context (RSP, RIP, RFLAGS)
     * 4. Register with Lattice IPC
     * 5. Add to scheduler ready queue
     *
     * @param desc Server descriptor
     * @return 0 on success, -1 on error
     */
    int spawn_server(const ServerDescriptor &desc) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "[SPAWN] Spawning server '%s' (PID %d)...\n", desc.name,
                 desc.pid);
        early_serial.write(buffer);

        // Allocate the Ring 3 stack.
        void *stack = kmalloc(desc.stack_size);
        if (!stack) {
            early_serial.write("[ERROR] Failed to allocate user stack\n");
            return -1;
        }

        // Stack grows downward on x86_64
        void *stack_top = static_cast<char *>(stack) + desc.stack_size;

        snprintf(buffer, sizeof(buffer), "  User stack: base=%p size=%lu top=%p\n", stack,
                 desc.stack_size, stack_top);
        early_serial.write(buffer);

        // Allocate the kernel stack used on privilege transitions.
        constexpr uint64_t KERNEL_STACK_SIZE = 4096; // 4 KB
        void *kernel_stack = kmalloc(KERNEL_STACK_SIZE);
        if (!kernel_stack) {
            early_serial.write("[ERROR] Failed to allocate kernel stack\n");
            return -1;
        }

        void *kernel_stack_top = static_cast<char *>(kernel_stack) + KERNEL_STACK_SIZE;

        snprintf(buffer, sizeof(buffer), "  Kernel stack: base=%p size=%lu top=%p\n", kernel_stack,
                 KERNEL_STACK_SIZE, kernel_stack_top);
        early_serial.write(buffer);

        // Create the process control block with its well-known PID.
        ProcessControlBlock *pcb = create_process_control_block(desc.pid);
        if (!pcb) {
            early_serial.write("[ERROR] Failed to create PCB\n");
            return -1;
        }

        pcb->name = pcb->name_storage.data();
        if (desc.name) {
            std::strncpy(pcb->name_storage.data(), desc.name, pcb->name_storage.size() - 1);
            pcb->name_storage.back() = '\0';
        } else {
            pcb->name_storage[0] = '\0';
        }
        pcb->state = ProcessState::READY;
        pcb->priority = desc.priority;

        // User stack
        pcb->stack_base = stack;
        pcb->stack_size = desc.stack_size;

        // Kernel stack used on privilege transitions.
        pcb->kernel_stack_base = kernel_stack;
        pcb->kernel_stack_size = KERNEL_STACK_SIZE;
        pcb->kernel_rsp = reinterpret_cast<uint64_t>(kernel_stack_top);

        // Initialize the Ring 3 CPU context for privilege separation.
        uint64_t entry_point = reinterpret_cast<uint64_t>(desc.entry_point);
        uint64_t stack_ptr = reinterpret_cast<uint64_t>(stack_top);

        // Initialize context for Ring 3 (user mode)
        pcb->context.initialize(entry_point, stack_ptr, 3); // Ring 3

        snprintf(buffer, sizeof(buffer), "  Context: RIP=%p RSP=%p RFLAGS=0x%lx\n",
                 (void *) pcb->context.rip, (void *) pcb->context.rsp, pcb->context.rflags);
        early_serial.write(buffer);

        // Register the server as a Lattice IPC receiver.
        lattice_register_server(desc.pid, desc.name);

        // Add the suspended server to the scheduler ready queue.
        scheduler_add_process(pcb);

        snprintf(buffer, sizeof(buffer), "[OK] Server '%s' spawned successfully\n", desc.name);
        early_serial.write(buffer);

        return 0;
    }

    /**
     * @brief Initialize all system servers
     *
     * Spawns VFS (PID 2), Process Manager (PID 3), and Memory Manager (PID 4).
     * Servers are created in READY state but not scheduled until kernel
     * enters main loop.
     *
     * @return 0 on success, -1 on error
     */
    int initialize_system_servers() {
        early_serial.write("\n========================================\n");
        early_serial.write("Initializing System Servers\n");
        early_serial.write("========================================\n");

        // Spawn VFS Server (PID 2)
        if (spawn_server(g_vfs_server_desc) != 0) {
            early_serial.write("[FATAL] Failed to spawn VFS server\n");
            return -1;
        }

        // Spawn Process Manager (PID 3)
        if (spawn_server(g_proc_mgr_desc) != 0) {
            early_serial.write("[FATAL] Failed to spawn Process Manager\n");
            return -1;
        }

        // Spawn Memory Manager (PID 4)
        if (spawn_server(g_mem_mgr_desc) != 0) {
            early_serial.write("[FATAL] Failed to spawn Memory Manager\n");
            return -1;
        }

        early_serial.write("========================================\n");
        early_serial.write("All system servers spawned\n");
        early_serial.write("========================================\n\n");

        return 0;
    }

    /**
     * @brief Spawn the init process (PID 1)
     *
     * Init is the first userspace process and the ancestor of all other
     * processes. It is responsible for:
     * - Starting user services
     * - Reaping orphaned processes
     * - Handling system shutdown
     *
     * @param init_path Path to the init ELF image
     * @param arguments Null-terminated initial argument vector
     * @param environment Null-terminated initial environment
     * @return 0 on success, -1 on error
     */
    int spawn_init_process(const char *init_path, const char *const *arguments,
                           const char *const *environment) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "[SPAWN] Spawning init process from '%s'...\n", init_path);
        early_serial.write(buffer);

#ifdef XINIM_ARCH_X86_64
        xinim::kernel::x86_64::Elf64UserImage user_image{};
        const auto load_error = xinim::kernel::x86_64::load_bootfs_elf64_user_image(
            init_path, user_image, arguments, environment);
        if (load_error != xinim::kernel::x86_64::Elf64LoadError::None) {
            early_serial.write("[ERROR] Failed to load init ELF64 image\n");
            return -1;
        }
#endif

        ProcessControlBlock *pcb = create_process_control_block(1);
        if (!pcb) {
            early_serial.write("[ERROR] Failed to create init PCB\n");
#ifdef XINIM_ARCH_X86_64
            xinim::kernel::x86_64::destroy_user_address_space(user_image.address_space);
#endif
            return -1;
        }

        constexpr uint64_t kInitKernelStackSize = 16U * 1024U;
        void *kernel_stack = kmalloc(kInitKernelStackSize);
        if (kernel_stack == nullptr) {
            early_serial.write("[ERROR] Failed to allocate init kernel stack\n");
#ifdef XINIM_ARCH_X86_64
            xinim::kernel::x86_64::destroy_user_address_space(user_image.address_space);
#endif
            return -1;
        }

        pcb->name = pcb->name_storage.data();
        std::strncpy(pcb->name_storage.data(), "init", pcb->name_storage.size() - 1);
        pcb->name_storage.back() = '\0';
        pcb->state = ProcessState::READY;
        pcb->priority = PRIO_USER_NORM;
        pcb->base_priority = PRIO_USER_NORM;
        pcb->stack_base = nullptr;
        pcb->stack_size = 0U;
        pcb->kernel_stack_base = kernel_stack;
        pcb->kernel_stack_size = kInitKernelStackSize;
        pcb->kernel_rsp = reinterpret_cast<uint64_t>(kernel_stack) + kInitKernelStackSize;
#ifdef XINIM_ARCH_X86_64
        pcb->address_space_root = user_image.address_space.root_physical;
        pcb->brk = user_image.program_break;
        pcb->minimum_break = user_image.program_break;
        pcb->context.initialize(user_image.entry_point, user_image.stack_pointer, 3);
        pcb->context.cr3 = user_image.address_space.root_physical;
#endif

        scheduler_add_process(pcb);

        early_serial.write("[OK] Init ELF64 process (PID 1) mapped for Ring 3\n");
        return 0;
    }

    // ============================================================================
    // Scheduler Entry Point
    // ============================================================================

    /**
     * @brief Start preemptive scheduler
     *
     * Delegates to the preemptive scheduler in scheduler.cpp, enabling
     * timer-based context switching between processes.
     *
     * @note This function never returns
     */
    [[noreturn]] void schedule_forever() {
        early_serial.write("\n========================================\n");
        early_serial.write("Starting Preemptive Scheduler\n");
        early_serial.write("========================================\n");

        start_scheduler();

        // Never returns
    }

    // ============================================================================
    // Server Descriptors
    // ============================================================================

    ServerDescriptor g_vfs_server_desc = {
        .pid = VFS_SERVER_PID, // 2
        .name = "vfs_server",
        .entry_point = vfs_server_main,
        .stack_size = 16384, // 16 KB
        .priority = 10       // High priority
    };

    ServerDescriptor g_proc_mgr_desc = {
        .pid = PROC_MGR_PID, // 3
        .name = "proc_mgr",
        .entry_point = proc_mgr_main,
        .stack_size = 16384, // 16 KB
        .priority = 10       // High priority
    };

    ServerDescriptor g_mem_mgr_desc = {
        .pid = MEM_MGR_PID, // 4
        .name = "mem_mgr",
        .entry_point = mem_mgr_main,
        .stack_size = 16384, // 16 KB
        .priority = 10       // High priority
    };

} // namespace xinim::kernel
