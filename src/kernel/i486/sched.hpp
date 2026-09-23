#pragma once
// Scheduler for the i486 ring3 subsystem.

#include "ring3_internal.hpp"

namespace xinim::i486::ring3 {

[[nodiscard]] uint32_t effective_quantum(const Process* process) noexcept;
void apply_scheduler_profile(Process* process,
                             uint32_t priority,
                             uint32_t base_priority,
                             uint32_t quantum_ticks,
                             uint16_t scheduler_domain) noexcept;
void wake_ready_waiters() noexcept;
void switch_process_context(Process* current, Process* next) noexcept;
uint32_t complete_syscall_return(Process* process, RegisterFrame* frame, uint32_t result) noexcept;
[[nodiscard]] Process* select_next_runnable(Process* preferred_current) noexcept;
[[noreturn]] void dispatch_process(Process* process) noexcept;
[[noreturn]] void dispatch_next_runnable(const char* rescue_reason) noexcept;
bool block_current_process_until_rescheduled(Process* process,
                                             WaitReason reason,
                                             uint64_t wake_tick) noexcept;

} // namespace xinim::i486::ring3
