/**
 * @file server_spawn.hpp
 * @brief Userspace server spawn infrastructure for XINIM microkernel
 *
 * This module provides the kernel-side infrastructure for spawning
 * userspace servers (VFS, Process Manager, Memory Manager) during
 * boot. Servers are assigned well-known PIDs and registered with
 * the Lattice IPC system.
 *
 * Boot sequence:
 * 1. Kernel initializes (hardware, memory, scheduler)
 * 2. spawn_server(VFS_SERVER_PID, vfs_server_main)
 * 3. spawn_server(PROC_MGR_PID, proc_mgr_main)
 * 4. spawn_server(MEM_MGR_PID, mem_mgr_main)
 * 5. spawn_init_process() - PID 1
 * 6. Kernel enters scheduler loop
 *
 * @ingroup kernel
 */

#ifndef XINIM_KERNEL_SERVER_SPAWN_HPP
#define XINIM_KERNEL_SERVER_SPAWN_HPP

#include "../include/xinim/core_types.hpp"
#include "../include/xinim/ipc/message_types.h"
#include <cstdint>

namespace xinim::kernel {

/**
 * @brief Server descriptor for boot-time initialization
 */
struct ServerDescriptor {
    xinim::pid_t pid;           /**< Well-known PID (2=VFS, 3=PROC, 4=MEM) */
    const char* name;           /**< Server name (for debugging) */
    void (*entry_point)();      /**< Server main function */
    uint64_t stack_size;        /**< Stack size in bytes */
    uint32_t priority;          /**< Scheduling priority */
};

/**
 * @brief Spawn a userspace server with a well-known PID
 *
 * Creates a new process/thread for the server, assigns the specified
 * PID, sets up the stack, and registers with Lattice IPC.
 *
 * @param desc Server descriptor
 * @return 0 on success, -1 on error
 *
 * @note This function is called during kernel boot, before the
 *       scheduler starts. Servers are created in a suspended state
 *       and will be scheduled once the kernel enters the main loop.
 */
int spawn_server(const ServerDescriptor& desc);

/**
 * @brief Spawn the init process (PID 1)
 *
 * The init process is the first userspace process and is responsible
 * for starting user-level services and handling orphaned processes.
 *
 * @param init_path Path to init binary (e.g., "/sbin/init")
 * @return 0 on success, -1 on error
 */
int spawn_init_process(const char* init_path);

/**
 * @brief Initialize all system servers
 *
 * Called during kernel boot to spawn VFS, Process Manager, and
 * Memory Manager servers.
 *
 * @return 0 on success, -1 on error
 */
int initialize_system_servers();

/**
 * @brief Well-known server descriptors
 */
extern ServerDescriptor g_vfs_server_desc;
extern ServerDescriptor g_proc_mgr_desc;
extern ServerDescriptor g_mem_mgr_desc;

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_SERVER_SPAWN_HPP */
