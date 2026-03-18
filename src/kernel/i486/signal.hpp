#pragma once
// Signal delivery for the i486 ring3 subsystem.

#include "ring3_internal.hpp"

namespace xinim::i486::ring3 {

void init_signal_state(Process* process) noexcept;
void send_signal_to_process(Process* target, uint32_t signum) noexcept;
bool deliver_one_signal(Process* process) noexcept;
bool is_default_terminate(uint32_t signum) noexcept;
bool is_default_ignore(uint32_t signum) noexcept;

[[nodiscard]] uint32_t sys_sigaction_impl(Process* process, RegisterFrame* frame) noexcept;
[[nodiscard]] uint32_t sys_signal_impl(Process* process, RegisterFrame* frame) noexcept;
[[nodiscard]] uint32_t sys_kill_impl(Process* process, RegisterFrame* frame) noexcept;
[[noreturn]] void sys_rt_sigreturn_impl(Process* process, RegisterFrame* frame) noexcept;
[[nodiscard]] uint32_t sys_rt_sigprocmask_compat(Process* process, RegisterFrame* frame) noexcept;

} // namespace xinim::i486::ring3
