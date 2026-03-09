#include "src/kernel/syscall_table.hpp"
#include "include/xinim/abi/lattice_changer.hpp"

#include <cstdlib>
#include <iostream>
#include <print>

namespace {

bool expect(bool condition, const char* message, int& failures) {
    if (!condition) {
        std::println(std::cerr, "FAIL: {}", message);
        ++failures;
        return false;
    }
    return true;
}

} // namespace

namespace xinim::kernel {

int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t count, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(fd + buf + count);
}

int64_t sys_write(uint64_t, uint64_t, uint64_t count, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(count);
}

int64_t sys_open(uint64_t, uint64_t flags, uint64_t mode, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(flags + mode);
}

int64_t sys_close(uint64_t fd, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(fd);
}

int64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(fd + offset + whence);
}

int64_t sys_pipe(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { return 22; }
int64_t sys_dup(uint64_t oldfd, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(oldfd + 100);
}
int64_t sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(oldfd + newfd);
}
int64_t sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(fd + cmd + arg);
}

int64_t sys_sigaction(uint64_t signum, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(signum);
}
int64_t sys_sigprocmask(uint64_t how, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(how);
}
int64_t sys_sigreturn(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { return 15; }
int64_t sys_kill(uint64_t pid, uint64_t sig, uint64_t, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(pid + sig);
}

int64_t sys_getpid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { return 4242; }
int64_t sys_getppid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { return 2121; }
int64_t sys_fork(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { return 57; }
int64_t sys_execve(uint64_t, uint64_t argv, uint64_t envp, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(argv + envp);
}
int64_t sys_wait4(uint64_t pid, uint64_t, uint64_t options, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(pid + options);
}
int64_t sys_exit(uint64_t status, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(status);
}

int64_t sys_setpgid(uint64_t pid, uint64_t pgid, uint64_t, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(pid + pgid);
}
int64_t sys_getpgid(uint64_t pid, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(pid);
}
int64_t sys_getpgrp(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { return 111; }
int64_t sys_setsid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { return 112; }
int64_t sys_getsid(uint64_t pid, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return static_cast<int64_t>(pid + 1);
}

} // namespace xinim::kernel

int main() {
    int failures = 0;
    constexpr int64_t expected_enosys = -38;

    expect(xinim::kernel::syscall_dispatch(
               static_cast<uint64_t>(xinim::kernel::SyscallNumber::GETPID), 0, 0, 0, 0, 0, 0) ==
               4242,
           "syscall_dispatch should route GETPID to the stubbed handler",
           failures);
    expect(xinim::kernel::syscall_dispatch(
               static_cast<uint64_t>(xinim::kernel::SyscallNumber::WRITE), 0, 0, 7, 0, 0, 0) == 7,
           "syscall_dispatch should route WRITE to the stubbed handler",
           failures);
    expect(xinim::kernel::syscall_dispatch(112, 0, 0, 0, 0, 0, 0) == 112,
           "syscall_dispatch should route SETSID to the stubbed handler",
           failures);
    expect(xinim::kernel::syscall_dispatch(200, 0, 0, 0, 0, 0, 0) == expected_enosys,
           "known-but-unimplemented syscalls should return -ENOSYS",
           failures);
    expect(xinim::kernel::syscall_dispatch(999, 0, 0, 0, 0, 0, 0) == expected_enosys,
           "out-of-range syscalls should return -ENOSYS",
           failures);
    expect(xinim::kernel::syscall_dispatch(
               xinim::abi::lattice::encode_tagged_syscall(
                   xinim::abi::lattice::SourceAbi::kLinux, 64, 0, 39),
               0, 0, 0, 0, 0, 0) == 4242,
           "tagged Linux64 getpid should translate to native GETPID",
           failures);
    expect(xinim::kernel::syscall_dispatch(
               xinim::abi::lattice::encode_tagged_syscall(
                   xinim::abi::lattice::SourceAbi::kLinux, 32, 0, 4),
               0, 0, 9, 0, 0, 0) == 9,
           "tagged Linux32 write should translate to native WRITE",
           failures);
    expect(xinim::kernel::syscall_dispatch(
               xinim::abi::lattice::encode_tagged_syscall(
                   xinim::abi::lattice::SourceAbi::kMach, 64, 0, 1),
               0, 0, 0, 0, 0, 0) == expected_enosys,
           "tagged Mach call should fail until IPC bridge exists",
           failures);

    expect(xinim::kernel::get_syscall_count(
               static_cast<uint64_t>(xinim::kernel::SyscallNumber::GETPID)) == 2,
           "GETPID should include native and translated Linux64 invocations",
           failures);
    expect(xinim::kernel::get_syscall_count(
               static_cast<uint64_t>(xinim::kernel::SyscallNumber::WRITE)) == 2,
           "WRITE should include native and translated Linux32 invocations",
           failures);
    expect(xinim::kernel::get_syscall_count(200) == 1,
           "known-but-unimplemented syscalls should still be counted",
           failures);
    expect(xinim::kernel::get_syscall_count(999) == 0,
           "out-of-range syscalls should not update a table entry",
           failures);
    expect(xinim::kernel::get_total_syscall_count() == 7,
           "total syscall count should include native and translated dispatches",
           failures);

    if (failures != 0) {
        std::println(std::cerr, "{} kernel syscall_table test(s) failed.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "ALL kernel syscall_table tests passed.");
    return EXIT_SUCCESS;
}
