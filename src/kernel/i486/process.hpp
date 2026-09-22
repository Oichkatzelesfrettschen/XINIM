#pragma once
// Process lifecycle management for the i486 ring3 subsystem.

#include "ring3_internal.hpp"

namespace xinim::i486::ring3 {

[[nodiscard]] uint32_t compute_segment_base(const Process& process) noexcept;
void activate_process(Process* process) noexcept;
void initialize_context(Process* process,
                        const elf32::UserImage& image,
                        uint32_t eax_value) noexcept;
[[nodiscard]] Process* allocate_process(uint32_t parent_pid) noexcept;
void destroy_process(Process* process) noexcept;
void initialize_supervised_session(Process& process, bool console_owner) noexcept;
void inherit_process_session(Process& child, const Process& parent) noexcept;
[[nodiscard]] uint32_t create_process_session(Process& process) noexcept;
[[nodiscard]] uint32_t query_process_session(const Process& process, uint32_t pid) noexcept;
[[nodiscard]] uint32_t set_process_group(Process& caller, uint32_t pid, uint32_t pgid) noexcept;
[[nodiscard]] bool owns_controlling_terminal(const Process& process) noexcept;
[[nodiscard]] uint32_t acquire_controlling_terminal(Process& process) noexcept;
void release_controlling_terminal(Process& process, bool continue_foreground = false) noexcept;
[[nodiscard]] uint32_t set_terminal_foreground(Process& process, int32_t pgid) noexcept;
[[noreturn]] void terminate_current_process_from_signal(uint32_t status) noexcept;
[[nodiscard]] Process* find_process(uint32_t pid) noexcept;
[[nodiscard]] Process* find_child(Process* parent, int32_t requested_pid) noexcept;
[[nodiscard]] bool has_child(Process* parent) noexcept;
[[nodiscard]] int process_slot_index(const Process* process) noexcept;
void clear_saved_kernel_stack(Process* process) noexcept;
[[noreturn]] void resume_waiting_parent(Process* parent) noexcept;
[[noreturn]] void resume_rescue_shell(const char* reason) noexcept;

} // namespace xinim::i486::ring3
