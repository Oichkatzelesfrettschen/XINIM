#include <stdint.h>
#include <string.h>
#include <xinim/sys/syscalls.h>
#include <xinim/abi/lattice_changer.hpp>
#include <xinim/core_types.hpp>
#include "../../../include/xinim/ipc/message_types.h"
#include "../../../include/xinim/ipc/vfs_protocol.hpp"
#include "../../../include/xinim/ipc/proc_protocol.hpp"
#include "../../../include/xinim/ipc/mm_protocol.hpp"
#include "../../../include/sys/type.hpp"
#include "../early/serial_16550.hpp"
#include "../time/monotonic.hpp"
#include "../lattice_ipc.hpp"
#include "../unified_scheduler.hpp"
#include "../process_lifecycle.hpp"
#include "console.hpp"

extern xinim::early::Serial16550 early_serial;

// Forward declarations for implementation functions
static int64_t sys_getpid_impl();

/**
 * @brief Send IPC request to a server and wait for response
 */
// Used by Phase 5 (P5-T04) when syscall routing is expanded.
[[maybe_unused]] static int send_ipc_request(xinim::pid_t caller_pid, xinim::pid_t server_pid,
                             const message& request, message& response) {
    int result = lattice::lattice_send(caller_pid, server_pid, request, lattice::IpcFlags::NONE);
    if (result != xinim::OK) return -1;
    result = lattice::lattice_recv(caller_pid, &response, lattice::IpcFlags::NONE);
    if (result != xinim::OK) return -1;
    return 0;
}

// Server PIDs for IPC routing (set by server_spawn.cpp)
// VFS_SERVER_PID is defined as macro (value 2) in message_types.h
static constexpr xinim::pid_t PM_SERVER_PID  = 3;

static uint64_t sys_debug_write_impl(const char* s, uint64_t n) {
    for (uint64_t i = 0; i < n; i++) early_serial.write_char(s[i]);
    return n;
}

static int64_t sys_getpid_impl() {
    return xinim::kernel::g_unified_scheduler.current_pid();
}

static int64_t sys_getppid_impl() {
    return 0; // init has no parent
}

/// Send a fully-constructed IPC request to a server and return its reply value.
/// The caller must populate request.m_type and all fields before calling.
static int64_t send_to_server(xinim::pid_t caller, xinim::pid_t server,
                              message& request) {
    message response{};
    if (send_ipc_request(caller, server, request, response) != 0) {
        return -1; // IPC transport error
    }
    return static_cast<int64_t>(response.m_type);
}

/// Route a syscall to a PM/generic server with three integer args.
/// Used for PM-side routing (fork, kill) where the message layout is simple.
static int64_t route_pm(xinim::pid_t caller, xinim::pid_t server,
                        int msg_type, uint64_t a0, uint64_t a1, uint64_t a2) {
    message request{};
    request.m_type = msg_type;
    request.m_u.m_m1.m1i1 = static_cast<int>(a0);
    request.m_u.m_m1.m1i2 = static_cast<int>(a1);
    request.m_u.m_m1.m1i3 = static_cast<int>(a2);
    return send_to_server(caller, server, request);
}

// Identity syscalls: always run as root (uid/gid = 0) until process
// credentials are tracked in PCB (Phase 8).
static int64_t sys_getuid_impl()  { return 0; }
static int64_t sys_getgid_impl()  { return 0; }
static int64_t sys_geteuid_impl() { return 0; }
static int64_t sys_getegid_impl() { return 0; }

// SYS_brk: bump-allocator stub -- Phase 8 replaces with real heap extension.
static constexpr uint64_t HEAP_BASE = 0x0000000001000000ULL; // 16 MB
static uint64_t heap_end = HEAP_BASE;
static int64_t sys_brk_impl(uint64_t addr) {
    if (addr == 0) return static_cast<int64_t>(heap_end);
    if (addr < HEAP_BASE) return -1; // ENOMEM
    heap_end = addr;
    return static_cast<int64_t>(heap_end);
}

static void sys_exit_impl(int status) {
    xinim::pid_t pid = xinim::kernel::g_unified_scheduler.current_pid();
    if (pid >= 0) {
        xinim::kernel::process_exit(pid, status);
        return;
    }
    // Fallback: halt if no current process
    while (true) { asm volatile("hlt"); }
}

static uint64_t resolve_syscall_no(uint64_t raw_no) {
#ifdef XINIM_ENABLE_LATTICE_CHANGER
    if (!xinim::abi::lattice::is_tagged(raw_no)) {
        return raw_no;
    }

    const auto tagged = xinim::abi::lattice::decode_tagged_syscall(raw_no);
    const auto translation = xinim::abi::lattice::transform_syscall(
        tagged.source,
        tagged.word_bits,
        tagged.arg_shape,
        tagged.foreign_syscall_no,
        xinim::abi::lattice::TransformTarget::kNativeSyscalls);
    if (!translation.supported || translation.estimated_cycles > 32) {
        return UINT64_MAX;
    }
    return translation.syscall_no;
#else
    return raw_no;
#endif
}

extern "C" uint64_t xinim_syscall_dispatch(uint64_t no,
    uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a3; (void)a4;
    const uint64_t resolved_no = resolve_syscall_no(no);

    xinim::pid_t caller = xinim::kernel::g_unified_scheduler.current_pid();
    if (caller < 0) caller = 1; // Fallback during early boot

    switch (resolved_no) {
        // --- Kernel-handled syscalls ---
        case SYS_debug_write:
            return sys_debug_write_impl(reinterpret_cast<const char*>(a0), a1);
        case SYS_getpid:
            return static_cast<uint64_t>(sys_getpid_impl());
        case SYS_getppid:
            return static_cast<uint64_t>(sys_getppid_impl());
        case SYS_exit:
            sys_exit_impl(static_cast<int>(a0));
            return 0; // unreachable

        // --- Kernel-handled identity / credential syscalls ---
        case SYS_getuid:  return static_cast<uint64_t>(sys_getuid_impl());
        case SYS_geteuid: return static_cast<uint64_t>(sys_geteuid_impl());
        case SYS_getgid:  return static_cast<uint64_t>(sys_getgid_impl());
        case SYS_getegid: return static_cast<uint64_t>(sys_getegid_impl());

        // --- Memory management (kernel-handled stubs) ---
        case SYS_brk:     return static_cast<uint64_t>(sys_brk_impl(a0));
        // mmap/munmap: stub -ENOSYS for now; Phase 8 will implement via MM server
        case SYS_mmap:    return static_cast<uint64_t>(-1);
        case SYS_munmap:  return static_cast<uint64_t>(0);  // no-op free

        // --- Routed to VFS server ---
        // Each case packs the message with the correct VFS_* type constant
        // and correct field layout matching vfs_server.cpp's expectations.
        case SYS_write: {
            // VFS_WRITE: m1i1=fd, m1i2=count, m1p1=buf
            message req{};
            req.m_type = VFS_WRITE;
            req.m_u.m_m1.m1i1 = static_cast<int>(a0);          // fd
            req.m_u.m_m1.m1i2 = static_cast<int>(a2);          // count
            req.m_u.m_m1.m1p1 = reinterpret_cast<char*>(a1);   // buf
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_read: {
            // VFS_READ: m1i1=fd, m1i2=count, m1p1=buf
            message req{};
            req.m_type = VFS_READ;
            req.m_u.m_m1.m1i1 = static_cast<int>(a0);          // fd
            req.m_u.m_m1.m1i2 = static_cast<int>(a2);          // count
            req.m_u.m_m1.m1p1 = reinterpret_cast<char*>(a1);   // buf
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_open: {
            // VFS_OPEN: m1p1=path, m1i1=flags, m1i2=mode
            message req{};
            req.m_type = VFS_OPEN;
            req.m_u.m_m1.m1p1 = reinterpret_cast<char*>(a0);   // path
            req.m_u.m_m1.m1i1 = static_cast<int>(a1);          // flags
            req.m_u.m_m1.m1i2 = static_cast<int>(a2);          // mode
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_close: {
            // VFS_CLOSE: m1i1=fd
            message req{};
            req.m_type = VFS_CLOSE;
            req.m_u.m_m1.m1i1 = static_cast<int>(a0);          // fd
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_lseek: {
            // VFS_LSEEK: m2i1=fd, m2i2=whence, m2l1=offset (int64_t)
            // Uses mess_2 to keep int fields and int64 field non-overlapping.
            message req{};
            req.m_type = VFS_LSEEK;
            req.m_u.m_m2.m2i1 = static_cast<int>(a0);          // fd
            req.m_u.m_m2.m2i2 = static_cast<int>(a2);          // whence
            req.m_u.m_m2.m2l1 = static_cast<int64_t>(a1);      // offset
            req.m_u.m_m2.m2i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_dup: {
            // VFS_DUP: m1i1=oldfd
            message req{};
            req.m_type = VFS_DUP;
            req.m_u.m_m1.m1i1 = static_cast<int>(a0);          // oldfd
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_dup2: {
            // VFS_DUP2: m1i1=oldfd, m1i2=newfd
            message req{};
            req.m_type = VFS_DUP2;
            req.m_u.m_m1.m1i1 = static_cast<int>(a0);          // oldfd
            req.m_u.m_m1.m1i2 = static_cast<int>(a1);          // newfd
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_pipe: {
            // VFS_PIPE: m1p1=pipefd (int[2])
            message req{};
            req.m_type = VFS_PIPE;
            req.m_u.m_m1.m1p1 = reinterpret_cast<char*>(a0);   // pipefd array
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_stat: {
            // VFS_STAT: m1p1=path
            message req{};
            req.m_type = VFS_STAT;
            req.m_u.m_m1.m1p1 = reinterpret_cast<char*>(a0);   // path
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_fstat: {
            // VFS_FSTAT: m1i1=fd
            message req{};
            req.m_type = VFS_FSTAT;
            req.m_u.m_m1.m1i1 = static_cast<int>(a0);          // fd
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_mkdir: {
            // VFS_MKDIR: m1p1=path, m1i1=mode
            message req{};
            req.m_type = VFS_MKDIR;
            req.m_u.m_m1.m1p1 = reinterpret_cast<char*>(a0);   // path
            req.m_u.m_m1.m1i1 = static_cast<int>(a1);          // mode
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }
        case SYS_unlink: {
            // VFS_UNLINK: m1p1=path
            message req{};
            req.m_type = VFS_UNLINK;
            req.m_u.m_m1.m1p1 = reinterpret_cast<char*>(a0);   // path
            req.m_u.m_m1.m1i3 = static_cast<int>(caller);
            return static_cast<uint64_t>(send_to_server(caller, VFS_SERVER_PID, req));
        }

        // --- Routed to Process Manager ---
        case SYS_fork:
            return static_cast<uint64_t>(route_pm(caller, PM_SERVER_PID,
                                                   PROC_FORK, 0, 0, 0));
        case SYS_execve:
            return static_cast<uint64_t>(route_pm(caller, PM_SERVER_PID,
                                                   PROC_EXEC, a0, a1, a2));
        case SYS_wait4: {
            // v1.2.0: Handle in kernel directly (PM server is stub)
            int wait_status = 0;
            xinim::pid_t child = xinim::kernel::process_wait(
                caller, static_cast<xinim::pid_t>(a0), &wait_status);
            if (a1 != 0) {
                // Write status to user pointer if provided
                auto* status_ptr = reinterpret_cast<int*>(a1);
                *status_ptr = wait_status;
            }
            return static_cast<uint64_t>(child);
        }
        case SYS_kill:
            return static_cast<uint64_t>(route_pm(caller, PM_SERVER_PID,
                                                   PROC_KILL, a0, a1, 0));

        default:
            Console::printf("Syscall %lu (resolved=%lu) not implemented\n", no, resolved_no);
            return static_cast<uint64_t>(-1);
    }
}
