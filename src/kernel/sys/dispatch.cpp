#include "../../../include/sys/type.hpp"
#include "../../../include/xinim/ipc/message_types.h"
#include "../../../include/xinim/ipc/mm_protocol.hpp"
#include "../../../include/xinim/ipc/proc_protocol.hpp"
#include "../../../include/xinim/ipc/vfs_protocol.hpp"
#include "../arch/x86_64/bootfs_syscalls.hpp"
#include "../arch/x86_64/memory_syscalls.hpp"
#include "../arch/x86_64/process_syscalls.hpp"
#include "../arch/x86_64/runtime_syscalls.hpp"
#include "../arch/x86_64/select_syscalls.hpp"
#include "../arch/x86_64/serial_terminal.hpp"
#include "../arch/x86_64/signal_syscalls.hpp"
#include "../arch/x86_64/syscall_frame.hpp"
#include "../early/serial_16550.hpp"
#include "../lattice_ipc.hpp"
#include "../process_lifecycle.hpp"
#include "../scheduler.hpp"
#include "../time/monotonic.hpp"
#include "../unified_scheduler.hpp"
#include "console.hpp"

#include <cerrno>
#include <stdint.h>
#include <string.h>
#include <xinim/abi/lattice_changer.hpp>
#include <xinim/abi/syscall_dispatch.h>
#include <xinim/core_types.hpp>
#include <xinim/sys/syscalls.h>

extern xinim::early::Serial16550 early_serial;

// Forward declarations for implementation functions
static int64_t sys_getpid_impl();

static uint64_t sys_debug_write_impl(const char *s, uint64_t n) {
    for (uint64_t i = 0; i < n; i++)
        early_serial.write_char(s[i]);
    return n;
}

static int64_t sys_getpid_impl() {
    return xinim::kernel::g_unified_scheduler.current_pid();
}

static int64_t sys_getppid_impl() {
    const xinim::kernel::ProcessControlBlock *process = xinim::kernel::get_current_process();
    return process != nullptr ? process->parent_pid : 0;
}

static int64_t sys_getuid_impl() {
    const auto *process = xinim::kernel::get_current_process();
    return process != nullptr ? process->real_user_id : 0;
}
static int64_t sys_getgid_impl() {
    const auto *process = xinim::kernel::get_current_process();
    return process != nullptr ? process->real_group_id : 0;
}
static int64_t sys_geteuid_impl() {
    const auto *process = xinim::kernel::get_current_process();
    return process != nullptr ? process->effective_user_id : 0;
}
static int64_t sys_getegid_impl() {
    const auto *process = xinim::kernel::get_current_process();
    return process != nullptr ? process->effective_group_id : 0;
}

static uint64_t resolve_syscall_no(uint64_t raw_no) {
#ifdef XINIM_ENABLE_LATTICE_CHANGER
    if (!xinim::abi::lattice::is_tagged(raw_no)) {
        return raw_no;
    }

    const auto tagged = xinim::abi::lattice::decode_tagged_syscall(raw_no);
    const auto translation = xinim::abi::lattice::transform_syscall(
        tagged.source, tagged.word_bits, tagged.arg_shape, tagged.foreign_syscall_no,
        xinim::abi::lattice::TransformTarget::kNativeSyscalls);
    if (!translation.supported || translation.estimated_cycles > 32) {
        return UINT64_MAX;
    }
    return translation.syscall_no;
#else
    return raw_no;
#endif
}

extern "C" uint64_t xinim_syscall_dispatch(uint64_t no, uint64_t a0, uint64_t a1, uint64_t a2,
                                           uint64_t a3, uint64_t a4, uint64_t a5) {
    const uint64_t resolved_no = resolve_syscall_no(no);

    switch (resolved_no) {
    // --- Kernel-handled syscalls ---
    case SYS_debug_write:
        return sys_debug_write_impl(reinterpret_cast<const char *>(a0), a1);
    case SYS_getpid:
        return static_cast<uint64_t>(sys_getpid_impl());
    case SYS_getppid:
        return static_cast<uint64_t>(sys_getppid_impl());
    case SYS_exit:
        xinim::kernel::x86_64::process_exit(static_cast<int>(a0));

    // --- Kernel-handled identity / credential syscalls ---
    case SYS_getuid:
        return static_cast<uint64_t>(sys_getuid_impl());
    case SYS_geteuid:
        return static_cast<uint64_t>(sys_geteuid_impl());
    case SYS_getgid:
        return static_cast<uint64_t>(sys_getgid_impl());
    case SYS_getegid:
        return static_cast<uint64_t>(sys_getegid_impl());

    // --- Memory management ---
    case SYS_brk:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_brk(a0));
    case SYS_mmap:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_mmap(a0, static_cast<size_t>(a1), static_cast<int>(a2),
                                                static_cast<int>(a3), static_cast<int>(a4), a5));
    case SYS_munmap:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_munmap(a0, static_cast<size_t>(a1)));
    case SYS_mremap:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_mremap(
            a0, static_cast<size_t>(a1), static_cast<size_t>(a2), a3, a4));

    case SYS_time:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_time(a0));
    case SYS_gettimeofday:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_gettimeofday(a0, a1));
    case SYS_clock_gettime:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_clock_gettime(static_cast<int>(a0), a1));
    case SYS_nanosleep:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_nanosleep(a0, a1));
    case SYS_alarm:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_alarm(static_cast<unsigned int>(a0)));
    case SYS_getrlimit:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_getrlimit(static_cast<int>(a0), a1));
    case SYS_setrlimit:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_setrlimit(static_cast<int>(a0), a1));
    case SYS_getrusage:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_getrusage(static_cast<int>(a0), a1));
    case SYS_umask:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_umask(static_cast<uint32_t>(a0)));
    case SYS_getpriority:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_getpriority(static_cast<int>(a0), static_cast<int>(a1)));
    case SYS_setpriority:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_setpriority(
            static_cast<int>(a0), static_cast<int>(a1), static_cast<int>(a2)));
    case SYS_setgroups:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_setgroups(static_cast<size_t>(a0), a1));
    case SYS_setresuid:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_setresuid(
            static_cast<int>(a0), static_cast<int>(a1), static_cast<int>(a2)));
    case SYS_setresgid:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_setresgid(
            static_cast<int>(a0), static_cast<int>(a1), static_cast<int>(a2)));
    case SYS_select:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_select(static_cast<int>(a0), a1, a2, a3, a4));

    // --- Routed to VFS server ---
    // Each case packs the message with the correct VFS_* type constant
    // and correct field layout matching vfs_server.cpp's expectations.
    case SYS_write: {
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_write(
            static_cast<int>(a0), a1, static_cast<uint32_t>(a2)));
    }
    case SYS_read: {
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_read(static_cast<int>(a0), a1,
                                                                        static_cast<uint32_t>(a2)));
    }
    case SYS_open: {
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_open(
            a0, static_cast<uint32_t>(a1), static_cast<uint32_t>(a2)));
    }
    case SYS_close: {
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_close(static_cast<int>(a0)));
    }
    case SYS_lseek: {
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_seek(
            static_cast<int>(a0), static_cast<int64_t>(a1), static_cast<int>(a2)));
    }
    case SYS_dup: {
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_duplicate(static_cast<int>(a0)));
    }
    case SYS_dup2: {
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::bootfs_duplicate_to(static_cast<int>(a0), static_cast<int>(a1)));
    }
    case SYS_fcntl:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::bootfs_fcntl(static_cast<int>(a0), static_cast<int>(a1), a2));
    case SYS_chown:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_chown(
            a0, static_cast<int64_t>(a1), static_cast<int64_t>(a2)));
    case SYS_fchown:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_fchown(
            static_cast<int>(a0), static_cast<int64_t>(a1), static_cast<int64_t>(a2)));
    case SYS_truncate:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::bootfs_truncate(a0, static_cast<int64_t>(a1)));
    case SYS_ftruncate:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_ftruncate(
            static_cast<int>(a0), static_cast<int64_t>(a1)));
    case SYS_flock:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::bootfs_flock(static_cast<int>(a0), static_cast<int>(a1)));
    case SYS_pipe: {
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_pipe(a0));
    }
    case SYS_ioctl:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::bootfs_ioctl(static_cast<int>(a0), a1, a2));
    case SYS_stat: {
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_stat(a0, a1));
    }
    case SYS_fstat: {
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_fstat(static_cast<int>(a0), a1));
    }
    case SYS_lstat:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_lstat(a0, a1));
    case SYS_symlink:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_symlink(a0, a1));
    case SYS_readlink:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::bootfs_readlink(a0, a1, static_cast<uint32_t>(a2)));
    case SYS_getdents:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_getdents(
            static_cast<int>(a0), a1, static_cast<uint32_t>(a2), false));
    case SYS_getdents64:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_getdents(
            static_cast<int>(a0), a1, static_cast<uint32_t>(a2), true));
    case SYS_mkdir: {
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::bootfs_mkdir(a0, static_cast<uint32_t>(a1)));
    }
    case SYS_unlink: {
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_unlink(a0));
    }
    case SYS_access:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_access(a0));
    case SYS_chdir:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_chdir(a0));
    case SYS_getcwd:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::bootfs_getcwd(a0, static_cast<uint32_t>(a1)));
    case SYS_rmdir:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_rmdir(a0));
    case SYS_rename:
        return static_cast<uint64_t>(xinim::kernel::x86_64::bootfs_rename(a0, a1));

    // --- Routed to Process Manager ---
    case SYS_fork:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_fork());
    case SYS_execve:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_exec(a0, a1, a2));
    case SYS_wait4: {
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_wait(static_cast<int>(a0), a1, static_cast<int>(a2)));
    }
    case SYS_kill:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_kill(static_cast<int>(a0), static_cast<int>(a1)));
    case SYS_signal:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_signal(static_cast<int>(a0), a1));
    case SYS_sigaction:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_sigaction(static_cast<int>(a0), a1, a2));
    case SYS_rt_sigprocmask:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_sigprocmask(
            static_cast<int>(a0), a1, a2, static_cast<size_t>(a3)));
    case SYS_sigsuspend:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_sigsuspend(a0, static_cast<size_t>(a1)));
    case SYS_rt_sigreturn:
        xinim::kernel::x86_64::process_sigreturn();
    case SYS_setpgid:
        return static_cast<uint64_t>(
            xinim::kernel::x86_64::process_setpgid(static_cast<int>(a0), static_cast<int>(a1)));
    case SYS_getpgid:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_getpgid(static_cast<int>(a0)));
    case SYS_getpgrp:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_getpgid(0));
    case SYS_setsid:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_setsid());
    case SYS_getsid:
        return static_cast<uint64_t>(xinim::kernel::x86_64::process_getsid(static_cast<int>(a0)));

    default:
        Console::printf("Syscall %lu (resolved=%lu) not implemented\n", no, resolved_no);
        return static_cast<uint64_t>(-ENOSYS);
    }
}

namespace xinim::kernel::x86_64 {
    namespace {
        const SyscallFrame *g_active_syscall_frame = nullptr;
    }

    const SyscallFrame *active_syscall_frame() noexcept {
        return g_active_syscall_frame;
    }

    extern "C" uint64_t xinim_syscall_dispatch_frame(SyscallFrame *frame) {
        g_active_syscall_frame = frame;
        reap_waited_processes();
        const uint64_t result = xinim_syscall_dispatch(
            frame->rax, frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);
        const uint64_t delivered_result = deliver_signals_from_syscall(*frame, result);
        g_active_syscall_frame = nullptr;
        return delivered_result;
    }
} // namespace xinim::kernel::x86_64
