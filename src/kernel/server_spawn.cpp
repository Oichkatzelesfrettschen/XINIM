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
 * Week 7 Limitations:
 * - Servers run in kernel mode (Ring 0) - no user/kernel separation yet
 * - Simple threading model (no full process isolation)
 * - Stack-based execution (no page table setup)
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "server_spawn.hpp"
#include "context.hpp"  // Week 8: Full CPU context for context switching
#include "early/serial_16550.hpp"
#include "../include/xinim/ipc/message_types.h"
#include <cstring>
#include <cstdint>

// External references
extern xinim::early::Serial16550 early_serial;

// Forward declarations for server entry points
extern "C" void vfs_server_main();
extern "C" void proc_mgr_main();
extern "C" void mem_mgr_main();

namespace xinim::kernel {

// ============================================================================
// Process Control Block (PCB) for Week 8
// ============================================================================

/**
 * @brief Process state enumeration
 */
enum class ProcessState {
    CREATED,    // PCB allocated, not yet ready
    READY,      // Ready to be scheduled
    RUNNING,    // Currently executing
    BLOCKED,    // Waiting for IPC or event
    ZOMBIE,     // Exited but not reaped
    DEAD        // Fully cleaned up
};

/**
 * @brief Simple Process Control Block for Week 7
 *
 * This is a minimal PCB for server threads. Full process support
 * (with page tables, FD tables, etc.) comes from the Process Manager
 * server itself.
 */
struct ProcessControlBlock {
    xinim::pid_t pid;           // Process ID
    const char* name;           // Human-readable name
    ProcessState state;         // Current state
    uint32_t priority;          // Scheduling priority (0-31)

    void* stack_base;           // Base of stack allocation
    uint64_t stack_size;        // Stack size in bytes

    CpuContext context;         // Saved CPU context

    // Linked list for scheduler queue
    ProcessControlBlock* next;
};

// ============================================================================
// Simple Kernel Heap Allocator (Week 7)
// ============================================================================

/**
 * @brief Simple bump allocator for kernel memory
 *
 * This is a TEMPORARY allocator for Week 7. Week 8+ will use the proper
 * kernel heap allocator.
 */
namespace {
    constexpr uint64_t KERNEL_HEAP_BASE = 0xFFFF'8000'0000'0000ULL;
    constexpr uint64_t KERNEL_HEAP_SIZE = 16 * 1024 * 1024;  // 16 MB
    uint64_t g_heap_current = KERNEL_HEAP_BASE;
    uint64_t g_heap_end = KERNEL_HEAP_BASE + KERNEL_HEAP_SIZE;
}

/**
 * @brief Allocate kernel memory (temporary implementation)
 */
static void* kmalloc(uint64_t size) {
    // Align to 16 bytes
    size = (size + 15) & ~15ULL;

    if (g_heap_current + size > g_heap_end) {
        early_serial.write("[ERROR] kernel heap exhausted\n");
        return nullptr;
    }

    void* ptr = reinterpret_cast<void*>(g_heap_current);
    g_heap_current += size;

    // Zero initialize
    memset(ptr, 0, size);

    return ptr;
}

// ============================================================================
// Simple Scheduler (Week 7)
// ============================================================================

namespace {
    ProcessControlBlock* g_ready_queue_head = nullptr;
    ProcessControlBlock* g_current_process = nullptr;
    uint64_t g_next_pid = 1;  // Start at 1 (0 is reserved for kernel)
}

/**
 * @brief Allocate a new PID
 */
static xinim::pid_t allocate_pid() {
    return static_cast<xinim::pid_t>(g_next_pid++);
}

/**
 * @brief Create a new PCB with a specific PID
 *
 * Used for well-known server PIDs (2, 3, 4).
 */
static ProcessControlBlock* create_pcb_with_pid(xinim::pid_t pid) {
    auto* pcb = static_cast<ProcessControlBlock*>(
        kmalloc(sizeof(ProcessControlBlock))
    );

    if (!pcb) {
        return nullptr;
    }

    pcb->pid = pid;
    pcb->name = nullptr;
    pcb->state = ProcessState::CREATED;
    pcb->priority = 10;  // Default priority
    pcb->stack_base = nullptr;
    pcb->stack_size = 0;
    pcb->next = nullptr;

    // Zero initialize context
    memset(&pcb->context, 0, sizeof(CpuContext));

    // Update g_next_pid if necessary
    if (static_cast<uint64_t>(pid) >= g_next_pid) {
        g_next_pid = static_cast<uint64_t>(pid) + 1;
    }

    return pcb;
}

/**
 * @brief Add process to scheduler ready queue
 */
static void scheduler_add_process(ProcessControlBlock* pcb) {
    if (!g_ready_queue_head) {
        g_ready_queue_head = pcb;
        pcb->next = nullptr;
    } else {
        // Insert at end (simple FIFO scheduling for Week 7)
        ProcessControlBlock* current = g_ready_queue_head;
        while (current->next) {
            current = current->next;
        }
        current->next = pcb;
        pcb->next = nullptr;
    }
}

/**
 * @brief Find PCB by PID
 */
static ProcessControlBlock* find_pcb_by_pid(xinim::pid_t pid) {
    ProcessControlBlock* current = g_ready_queue_head;
    while (current) {
        if (current->pid == pid) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}

// ============================================================================
// Lattice IPC Registration (Stub for Week 7)
// ============================================================================

/**
 * @brief Register server with Lattice IPC system
 *
 * This registers the server's PID with the IPC subsystem so that
 * other processes can send messages to it.
 */
static void lattice_register_server(xinim::pid_t pid, const char* name) {
    // TODO: Implement full IPC registration in lattice_ipc.cpp
    // For Week 7, this is a no-op since servers aren't fully isolated yet

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[IPC] Registered server '%s' with PID %d\n", name, pid);
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
int spawn_server(const ServerDescriptor& desc) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[SPAWN] Spawning server '%s' (PID %d)...\n", desc.name, desc.pid);
    early_serial.write(buffer);

    // Step 1: Allocate stack
    void* stack = kmalloc(desc.stack_size);
    if (!stack) {
        early_serial.write("[ERROR] Failed to allocate stack\n");
        return -1;
    }

    // Stack grows downward on x86_64
    void* stack_top = static_cast<char*>(stack) + desc.stack_size;

    snprintf(buffer, sizeof(buffer),
             "  Stack: base=%p size=%lu top=%p\n",
             stack, desc.stack_size, stack_top);
    early_serial.write(buffer);

    // Step 2: Create PCB with well-known PID
    ProcessControlBlock* pcb = create_pcb_with_pid(desc.pid);
    if (!pcb) {
        early_serial.write("[ERROR] Failed to create PCB\n");
        return -1;
    }

    pcb->name = desc.name;
    pcb->state = ProcessState::READY;
    pcb->priority = desc.priority;
    pcb->stack_base = stack;
    pcb->stack_size = desc.stack_size;

    // Step 3: Set up initial CPU context
    // Week 8: Servers run in Ring 0 initially, will transition to Ring 3 later
    uint64_t entry_point = reinterpret_cast<uint64_t>(desc.entry_point);
    uint64_t stack_ptr = reinterpret_cast<uint64_t>(stack_top);

    // Use CpuContext::initialize() for clean setup
    // Ring 0 for Week 8 (will be Ring 3 once privilege separation is complete)
    pcb->context.initialize(entry_point, stack_ptr, 0);

    snprintf(buffer, sizeof(buffer),
             "  Context: RIP=%p RSP=%p RFLAGS=0x%lx\n",
             (void*)pcb->context.rip,
             (void*)pcb->context.rsp,
             pcb->context.rflags);
    early_serial.write(buffer);

    // Step 4: Register with Lattice IPC
    lattice_register_server(desc.pid, desc.name);

    // Step 5: Add to scheduler (but don't start yet)
    scheduler_add_process(pcb);

    snprintf(buffer, sizeof(buffer),
             "[OK] Server '%s' spawned successfully\n", desc.name);
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
 * Week 7 Limitation: Init is not yet implemented. This function
 * creates a placeholder PCB but does not load an ELF binary.
 *
 * @param init_path Path to init binary (e.g., "/sbin/init")
 * @return 0 on success, -1 on error
 */
int spawn_init_process(const char* init_path) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[SPAWN] Spawning init process from '%s'...\n", init_path);
    early_serial.write(buffer);

    // Week 7 limitation: We don't yet have ELF loading or VFS access
    // during boot, so we just create a placeholder

    early_serial.write("[WARN] Init process spawning not yet implemented\n");
    early_serial.write("[INFO] System will run servers only (no userspace init)\n");

    return -1;  // Not an error, just not implemented yet
}

// ============================================================================
// Simple Scheduler Entry Point (Week 7)
// ============================================================================

/**
 * @brief Simple round-robin scheduler loop
 *
 * This is a VERY simple cooperative scheduler for Week 7. Each server
 * runs its main loop and voluntarily yields control.
 *
 * Week 8+ will implement:
 * - Preemptive scheduling with timer interrupts
 * - Priority-based scheduling
 * - CPU affinity for SMP
 * - Real-time guarantees
 */
[[noreturn]] void schedule_forever() {
    early_serial.write("\n========================================\n");
    early_serial.write("Starting Scheduler\n");
    early_serial.write("========================================\n");

    if (!g_ready_queue_head) {
        early_serial.write("[WARN] No processes to schedule\n");
        early_serial.write("Entering idle loop...\n");

        for(;;) {
#ifdef XINIM_ARCH_X86_64
            __asm__ volatile ("hlt");
#elif defined(XINIM_ARCH_ARM64)
            __asm__ volatile ("wfi");
#endif
        }
    }

    // For Week 7, we'll just call each server's main function sequentially
    // This is NOT real scheduling, but allows servers to initialize

    early_serial.write("\n[SCHEDULER] Calling server entry points...\n");

    ProcessControlBlock* current = g_ready_queue_head;
    while (current) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer),
                 "[SCHEDULER] Starting '%s' (PID %d)\n",
                 current->name, current->pid);
        early_serial.write(buffer);

        current->state = ProcessState::RUNNING;
        g_current_process = current;

        // Call server entry point
        // Note: In Week 7, servers run to completion (cooperative)
        // Week 8 will implement proper preemption
        auto entry_fn = reinterpret_cast<void(*)()>(current->context.rip);

        // TEMPORARY: Just call the function directly
        // Week 8 will properly set up stack and jump to RIP
        entry_fn();

        // If server returns (shouldn't happen), mark as ZOMBIE
        current->state = ProcessState::ZOMBIE;

        snprintf(buffer, sizeof(buffer),
                 "[WARN] Server '%s' exited unexpectedly\n", current->name);
        early_serial.write(buffer);

        current = current->next;
    }

    early_serial.write("\n[SCHEDULER] All servers completed\n");
    early_serial.write("Entering idle loop...\n");

    for(;;) {
#ifdef XINIM_ARCH_X86_64
        __asm__ volatile ("hlt");
#elif defined(XINIM_ARCH_ARM64)
        __asm__ volatile ("wfi");
#endif
    }
}

// ============================================================================
// Server Descriptors
// ============================================================================

ServerDescriptor g_vfs_server_desc = {
    .pid = VFS_SERVER_PID,           // 2
    .name = "vfs_server",
    .entry_point = vfs_server_main,
    .stack_size = 16384,             // 16 KB
    .priority = 10                   // High priority
};

ServerDescriptor g_proc_mgr_desc = {
    .pid = PROC_MGR_PID,             // 3
    .name = "proc_mgr",
    .entry_point = proc_mgr_main,
    .stack_size = 16384,             // 16 KB
    .priority = 10                   // High priority
};

ServerDescriptor g_mem_mgr_desc = {
    .pid = MEM_MGR_PID,              // 4
    .name = "mem_mgr",
    .entry_point = mem_mgr_main,
    .stack_size = 16384,             // 16 KB
    .priority = 10                   // High priority
};

} // namespace xinim::kernel
