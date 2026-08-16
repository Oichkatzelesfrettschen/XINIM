#pragma once

#include <cstdint>

namespace xinim::kernel {
    struct ProcessControlBlock;
}

namespace xinim::kernel::x86_64 {

    [[nodiscard]] int64_t process_fork() noexcept;
    [[nodiscard]] int64_t process_exec(uintptr_t pathname, uintptr_t argv,
                                       uintptr_t environment) noexcept;
    [[nodiscard]] int64_t process_wait(int child_pid, uintptr_t status_address,
                                       int options) noexcept;
    [[nodiscard]] int64_t process_brk(uintptr_t address) noexcept;
    [[nodiscard]] int64_t process_setpgid(int pid, int pgid) noexcept;
    [[nodiscard]] int64_t process_getpgid(int pid) noexcept;
    [[nodiscard]] int64_t process_setsid() noexcept;
    [[nodiscard]] int64_t process_getsid(int pid) noexcept;
    [[noreturn]] void process_exit(int status) noexcept;
    [[noreturn]] void process_exit_for_signal(int signum) noexcept;
    [[noreturn]] void process_stop(int signum) noexcept;
    void process_continue(ProcessControlBlock &process) noexcept;
    [[noreturn]] void process_block_for_io() noexcept;
    [[noreturn]] void process_block_for_select(uint64_t deadline_tick,
                                               uintptr_t timeout_address) noexcept;
    [[noreturn]] void process_sleep_until(uint64_t deadline_tick,
                                          uintptr_t remaining_address) noexcept;
    [[noreturn]] void process_block_for_signal() noexcept;
    void process_interrupt_sleep(ProcessControlBlock &process) noexcept;
    void process_interrupt_select(ProcessControlBlock &process) noexcept;
    void wake_io_waiters() noexcept;
    void reap_waited_processes() noexcept;

} // namespace xinim::kernel::x86_64
