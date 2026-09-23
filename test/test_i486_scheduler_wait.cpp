#include "../src/kernel/i486/sched.hpp"
#include "../src/kernel/i486/process.hpp"
#include "../src/kernel/i486/tcp.hpp"
#include "i486_check.hpp"

#include <cstdlib>
#include <cstring>

namespace {
bool input_ready = false;
bool pipe_eof_ready = false;
unsigned switch_count = 0U;
const xinim::i486::ring3::UserContext* switched_context = nullptr;
uint32_t incoming_kernel_stack = 0U;
}

namespace xinim::i486::console {
bool tty_has_input() noexcept { return input_ready; }
void write_string(const char*) noexcept {}
void write_dec32(uint32_t) noexcept {}
void newline() noexcept {}
}

namespace xinim::kernel::bootfs {
bool consume_pipe_eof_event() noexcept { return pipe_eof_ready; }
}

namespace xinim::i486::net {
TcpState tcp_state(int) noexcept { return TcpState::Closed; }
}

namespace xinim::i486::ring3 {
Process g_processes[kMaxProcesses]{};
Process* g_current_process = nullptr;
uint64_t g_scheduler_ticks = 0U;
int process_slot_index(const Process* process) noexcept {
    return process == nullptr ? -1 : static_cast<int>(process - g_processes);
}
void activate_process(Process* process) noexcept { g_current_process = process; }
void clear_saved_kernel_stack(Process* process) noexcept { process->saved_kernel_esp = 0U; }
[[noreturn]] void resume_rescue_shell(const char*) noexcept { std::abort(); }
extern "C" void i486_switch_process_context(const UserContext* context,
                                              uint32_t* saved_esp,
                                              uint32_t next_kernel_esp) noexcept {
    ++switch_count;
    switched_context = context;
    incoming_kernel_stack = next_kernel_esp;
    *saved_esp = 0x1234U;
}
}

int main() {
    using namespace xinim::i486::ring3;
    Process& caller = g_processes[0];
    Process& other = g_processes[1];
    caller.in_use = true;
    caller.pid = 1U;
    caller.priority = 0U;
    caller.state = ProcessState::Runnable;
    caller.context.eax = SYS_read;
    other.in_use = true;
    other.pid = 2U;
    other.priority = 1U;
    other.state = ProcessState::Runnable;
    g_current_process = &caller;

    // A UART arrival between an empty read and the scheduler's readiness scan
    // wakes the caller while its syscall still owns the active kernel stack.
    input_ready = true;
    CHECK(!block_current_process_until_rescheduled(&caller, WaitReason::ConsoleInput, 0U));
    CHECK(switch_count == 0U);
    CHECK(caller.saved_kernel_esp == 0U);
    CHECK(g_current_process == &caller);
    CHECK(caller.state == ProcessState::Runnable);
    CHECK(caller.wait_reason == WaitReason::None);

    input_ready = false;
    pipe_eof_ready = true;
    CHECK(!block_current_process_until_rescheduled(&caller, WaitReason::PipeIO, 0U));
    CHECK(switch_count == 0U);
    pipe_eof_ready = false;
    g_scheduler_ticks = 8U;
    CHECK(!block_current_process_until_rescheduled(&caller, WaitReason::ConsoleInput, 8U));
    CHECK(switch_count == 0U);
    CHECK(caller.wake_tick == 0U);

    input_ready = true;
    caller.signals.pending = 1U << 2U;
    CHECK(block_current_process_until_rescheduled(&caller, WaitReason::ConsoleInput, 0U));
    CHECK(switch_count == 0U);
    caller.signals.blocked = caller.signals.pending;
    CHECK(!block_current_process_until_rescheduled(&caller, WaitReason::ConsoleInput, 0U));
    CHECK(switch_count == 0U);
    caller.signals.pending = 1U << kSigKill;
    caller.signals.blocked = caller.signals.pending;
    CHECK(block_current_process_until_rescheduled(&caller, WaitReason::ConsoleInput, 0U));
    CHECK(switch_count == 0U);
    caller.signals.pending = 0U;
    caller.signals.blocked = 0U;

    // An actual wait still saves the caller's continuation and enters the
    // runnable peer. The assembly spy returns at the saved continuation seam.
    input_ready = false;
    CHECK(!block_current_process_until_rescheduled(&caller, WaitReason::ConsoleInput, 0U));
    CHECK(switch_count == 1U);
    CHECK(switched_context == &other.context);
    CHECK(incoming_kernel_stack == 0U);
    CHECK(g_current_process == &caller);
    CHECK(caller.saved_kernel_esp == 0U);
    CHECK(caller.state == ProcessState::Runnable);
    CHECK(caller.wait_reason == WaitReason::None);
    other.saved_kernel_esp = 0x76543210U;
    CHECK(!block_current_process_until_rescheduled(&caller, WaitReason::ConsoleInput, 0U));
    CHECK(switch_count == 2U);
    CHECK(incoming_kernel_stack == 0x76543210U);
    CHECK(other.saved_kernel_esp == 0U);
    CHECK(caller.saved_kernel_esp == 0U);
    CHECK(g_current_process == &caller);
    std::puts("i486 scheduler wait completion and actual-switch checks passed");
    return 0;
}
