#pragma once

#include <cstddef>
#include <cstdint>

namespace xinim::kernel {
    struct CpuContext_x86_64;
    struct ProcessControlBlock;
    struct X86_64InterruptFrame;
} // namespace xinim::kernel

namespace xinim::kernel::x86_64 {

    struct SyscallFrame;

    [[nodiscard]] int64_t process_kill(int pid, int signum) noexcept;
    [[nodiscard]] int64_t process_signal(int signum, uint64_t handler) noexcept;
    [[nodiscard]] int64_t process_sigaction(int signum, uintptr_t action,
                                            uintptr_t old_action) noexcept;
    [[nodiscard]] int64_t process_sigprocmask(int how, uintptr_t set, uintptr_t old_set,
                                              std::size_t signal_set_size) noexcept;
    [[nodiscard]] int64_t process_sigsuspend(uintptr_t set, std::size_t signal_set_size) noexcept;
    [[noreturn]] void process_sigreturn() noexcept;

    [[nodiscard]] uint64_t deliver_signals_from_syscall(SyscallFrame &frame,
                                                        uint64_t syscall_result) noexcept;
    void deliver_signals_from_timer(ProcessControlBlock &process, X86_64InterruptFrame &frame,
                                    void *fxsave_image) noexcept;
    void deliver_signals_to_context(ProcessControlBlock &process,
                                    CpuContext_x86_64 &context) noexcept;

} // namespace xinim::kernel::x86_64
