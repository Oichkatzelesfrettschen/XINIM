#include "../src/kernel/i486/kutil.hpp"
#include "../src/kernel/i486/process.hpp"
#include "../src/kernel/i486/sched.hpp"
#include "../src/kernel/i486/signal.hpp"
#include "../src/kernel/i486/tcp.hpp"
#include "i486_check.hpp"

#include <csetjmp>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {
std::jmp_buf dispatch_boundary;
constexpr int kReturnedToUser = 1;
constexpr int kTerminated = 2;
constexpr int kCompletedSyscall = 3;
constexpr uint32_t kSyscallResult = static_cast<uint32_t>(-4);
struct SyscallFrame {
    xinim::i486::ring3::RegisterFrame registers;
    uint32_t return_words[5];
};
static_assert(offsetof(SyscallFrame, return_words) ==
              sizeof(xinim::i486::ring3::RegisterFrame));
SyscallFrame syscall_frame{};
uint32_t completed_result = 0U;
const xinim::i486::ring3::UserContext* resumed_context = nullptr;
xinim::i486::ring3::Process* terminated_process = nullptr;
uint32_t termination_status = 0U;
bool input_ready = false;
uint32_t timer_eoi_count = 0U;
}

namespace xinim::i486::console {
bool tty_has_input() noexcept { return input_ready; }
void tty_poll_input() noexcept {}
uint32_t consume_pending_tty_signal() noexcept { return 0U; }
}

namespace xinim::i486::tty {
uint32_t consume_pending_ldisc_signal() noexcept { return 0U; }
}

namespace xinim::kernel::bootfs {
bool consume_pipe_eof_event() noexcept { return false; }
}

namespace xinim::i486::net {
TcpState tcp_state(int) noexcept { return TcpState::Closed; }
}

namespace xinim::i486::ring3 {
Process g_processes[kMaxProcesses]{};
Process* g_current_process = nullptr;
uint64_t g_scheduler_ticks = 0U;
extern "C" [[noreturn]] void i486_handle_timer_irq(RegisterFrame* frame) noexcept;
void send_timer_eoi() noexcept { ++timer_eoi_count; }

int process_slot_index(const Process* process) noexcept {
    return process == nullptr ? -1 : static_cast<int>(process - g_processes);
}
void activate_process(Process* process) noexcept { g_current_process = process; }
Process* find_process(uint32_t pid) noexcept {
    for (auto& process : g_processes) {
        if (process.in_use && process.pid == pid) {
            return &process;
        }
    }
    return nullptr;
}
SupervisedService* find_supervised_service_by_process(const Process*) noexcept {
    return nullptr;
}
[[noreturn]] void resume_rescue_shell(const char*) noexcept { std::abort(); }
[[noreturn]] void resume_waiting_parent(Process*) noexcept { std::abort(); }
extern "C" void i486_resume_saved_kernel_stack(uint32_t) noexcept { std::abort(); }
extern "C" void i486_resume_user_context(const UserContext* context) noexcept {
    resumed_context = context;
    std::longjmp(dispatch_boundary, kReturnedToUser);
}
[[noreturn]] void terminate_current_process_from_signal(uint32_t status) noexcept {
    terminated_process = g_current_process;
    termination_status = status;
    std::longjmp(dispatch_boundary, kTerminated);
}

}

namespace {
void prepare_processes() {
    using namespace xinim::i486::ring3;
    std::memset(g_processes, 0, sizeof(g_processes));
    for (size_t index = 0U; index < 2U; ++index) {
        Process& process = g_processes[index];
        process.in_use = true;
        process.pid = static_cast<uint32_t>(index + 1U);
        process.state = ProcessState::Runnable;
        process.priority = 16U;
    }
    resumed_context = nullptr;
    terminated_process = nullptr;
    termination_status = 0U;
    g_current_process = &g_processes[0];
    syscall_frame = {};
    syscall_frame.registers.eax = SYS_read;
    syscall_frame.return_words[0] = 0x00401234U;
    syscall_frame.return_words[1] = kUserCodeSelector;
    syscall_frame.return_words[2] = kUserEflags;
    syscall_frame.return_words[3] = 0x007FF000U;
    syscall_frame.return_words[4] = kUserDataSelector;
    completed_result = 0U;
    input_ready = false;
    timer_eoi_count = 0U;
    g_scheduler_ticks = 0U;
}

int capture_dispatch() {
    switch (setjmp(dispatch_boundary)) {
    case 0:
        xinim::i486::ring3::dispatch_process(&xinim::i486::ring3::g_processes[0]);
    case kReturnedToUser:
        return kReturnedToUser;
    case kTerminated:
        return kTerminated;
    default:
        std::abort();
    }
}

int capture_invalid_sigreturn() {
    switch (setjmp(dispatch_boundary)) {
    case 0: {
        xinim::i486::ring3::RegisterFrame frame{};
        xinim::i486::ring3::sys_rt_sigreturn_impl(
            &xinim::i486::ring3::g_processes[0], &frame);
    }
    case kReturnedToUser:
        return kReturnedToUser;
    case kTerminated:
        return kTerminated;
    default:
        std::abort();
    }
}

int capture_syscall_return() {
    switch (setjmp(dispatch_boundary)) {
    case 0:
        completed_result = xinim::i486::ring3::complete_syscall_return(
            &xinim::i486::ring3::g_processes[0], &syscall_frame.registers, kSyscallResult);
        return kCompletedSyscall;
    case kReturnedToUser:
        return kReturnedToUser;
    case kTerminated:
        return kTerminated;
    default:
        std::abort();
    }
}

int capture_timer_irq() {
    switch (setjmp(dispatch_boundary)) {
    case 0:
        xinim::i486::ring3::i486_handle_timer_irq(&syscall_frame.registers);
    case kReturnedToUser:
        return kReturnedToUser;
    default:
        std::abort();
    }
}
}

int main() {
    using namespace xinim::i486::ring3;

    // The actual dispatcher must route around a process stopped by terminal
    // signal delivery, rather than enter its assembly user-return boundary.
    for (const uint32_t signal : {kSigTtin, kSigTtou}) {
        prepare_processes();
        g_processes[0].signals.pending = 1U << signal;
        CHECK(capture_dispatch() == kReturnedToUser);
        CHECK(g_processes[0].state == ProcessState::Stopped);
        CHECK(g_processes[0].exit_status == ((signal << 8U) | 0x7FU));
        CHECK(resumed_context == &g_processes[1].context);
        CHECK(g_current_process == &g_processes[1]);
        CHECK(terminated_process == nullptr);
    }

    // Continuing a stopped process changes its scheduling state even when
    // its SIGCONT disposition is ignored or signal delivery is blocked.
    for (const bool ignored : {false, true}) {
        for (const bool blocked : {false, true}) {
            prepare_processes();
            Process& stopped = g_processes[0];
            stopped.state = ProcessState::Stopped;
            stopped.signals.handlers[kSigCont].handler = ignored ? kSigIgn : kSigDfl;
            stopped.signals.blocked = blocked ? (1U << kSigCont) : 0U;
            send_signal_to_process(&stopped, kSigCont);
            CHECK(stopped.state == ProcessState::Runnable);
            CHECK(capture_dispatch() == kReturnedToUser);
            CHECK(resumed_context == &stopped.context);
            CHECK(terminated_process == nullptr);
        }
    }

    // Signal death must use the common teardown entry point. Its spy captures
    // control before real descriptor cleanup and service restart would run.
    for (const uint32_t signal : {kSigHup, kSigTerm}) {
        prepare_processes();
        g_processes[0].signals.pending = 1U << signal;
        CHECK(capture_dispatch() == kTerminated);
        CHECK(terminated_process == &g_processes[0]);
        CHECK(termination_status == 128U + signal);
        CHECK(resumed_context == nullptr);
    }

    prepare_processes();
    g_processes[0].signals.pending = 1U << kSigTerm;
    g_processes[0].signals.handlers[kSigTerm].handler = xinim::i486::elf32::kUserVirtualBase;
    CHECK(capture_dispatch() == kTerminated);
    CHECK(termination_status == 128U + kSigSegv);
    CHECK(terminated_process == &g_processes[0]);
    CHECK(resumed_context == nullptr);

    prepare_processes();
    CHECK(capture_invalid_sigreturn() == kTerminated);
    CHECK(termination_status == 128U + kSigSegv);
    CHECK(terminated_process == &g_processes[0]);
    CHECK(resumed_context == nullptr);

    // Fast syscall returns preserve the result when pending delivery is absent,
    // masked, or deferred by an active handler.
    for (unsigned scenario = 0U; scenario < 3U; ++scenario) {
        prepare_processes();
        if (scenario != 0U) {
            g_processes[0].signals.pending = 1U << kSigTerm;
        }
        g_processes[0].signals.blocked = scenario == 1U ? (1U << kSigTerm) : 0U;
        g_processes[0].signals.in_handler = scenario == 2U;
        CHECK(capture_syscall_return() == kCompletedSyscall);
        CHECK(completed_result == kSyscallResult);
        CHECK(resumed_context == nullptr);
        CHECK(terminated_process == nullptr);
    }

    prepare_processes();
    g_processes[0].signals.pending = 1U << kSigKill;
    CHECK(capture_syscall_return() == kTerminated);
    CHECK(terminated_process == &g_processes[0]);
    CHECK(termination_status == 128U + kSigKill);
    CHECK(resumed_context == nullptr);

    // Signal delivery after a syscall must use the completed result and the
    // hardware return frame, rather than the stale syscall-entry register copy.
    prepare_processes();
    g_processes[0].signals.pending = 1U << kSigTerm;
    g_processes[0].signals.handlers[kSigTerm].handler = kSigIgn;
    CHECK(capture_syscall_return() == kReturnedToUser);
    CHECK(resumed_context == &g_processes[0].context);
    CHECK(resumed_context->eax == kSyscallResult);
    CHECK(resumed_context->eip == syscall_frame.return_words[0]);
    CHECK(resumed_context->esp == syscall_frame.return_words[3]);
    CHECK(resumed_context->cs == kUserCodeSelector);
    CHECK(resumed_context->ss == kUserDataSelector);

    prepare_processes();
    g_processes[0].signals.pending = 1U << kSigTtin;
    CHECK(capture_syscall_return() == kReturnedToUser);
    CHECK(g_processes[0].state == ProcessState::Stopped);
    CHECK(resumed_context == &g_processes[1].context);
    CHECK(g_processes[0].context.eax == kSyscallResult);
    CHECK(g_processes[0].context.eip == syscall_frame.return_words[0]);
    CHECK(g_processes[0].context.esp == syscall_frame.return_words[3]);

    // Wait interruption follows the pending signal's disposition and mask;
    // an ignored child notification must allow the child-state wait to proceed.
    prepare_processes();
    Process& waiter = g_processes[0];
    waiter.signals.pending = 1U << kSigChld;
    CHECK(!has_interrupting_signal(waiter));
    waiter.signals.handlers[kSigChld].handler = kSigIgn;
    CHECK(!has_interrupting_signal(waiter));
    waiter.signals.handlers[kSigChld].handler = xinim::i486::elf32::kUserVirtualBase;
    CHECK(has_interrupting_signal(waiter));
    waiter.signals.blocked = 1U << kSigChld;
    CHECK(!has_interrupting_signal(waiter));
    waiter.signals.blocked = 0U;
    waiter.signals.pending = 1U << kSigHup;
    waiter.signals.handlers[kSigHup].handler = kSigIgn;
    CHECK(!has_interrupting_signal(waiter));
    waiter.signals.handlers[kSigHup].handler = kSigDfl;
    CHECK(has_interrupting_signal(waiter));

    // Saturated CPU-bound peers rotate through occupied slots and wrap after
    // quantum expiry; a fixed slot-zero scan would starve later peers forever.
    prepare_processes();
    g_processes[1].state = ProcessState::Stopped;
    const size_t runnable_slots[] = {0U, 2U, kMaxProcesses - 1U};
    for (const size_t slot : runnable_slots) {
        Process& process = g_processes[slot];
        process.in_use = true;
        process.pid = static_cast<uint32_t>(slot + 1U);
        process.state = ProcessState::Runnable;
        apply_scheduler_profile(&process, 48U, 48U, 1U, 0U);
    }
    for (size_t tick = 0U; tick < 12U; ++tick) {
        CHECK(capture_timer_irq() == kReturnedToUser);
        const size_t expected_slot = runnable_slots[(tick + 1U) % 3U];
        CHECK(g_current_process == &g_processes[expected_slot]);
        CHECK(resumed_context == &g_processes[expected_slot].context);
        CHECK(g_current_process->priority == 48U);
        CHECK(timer_eoi_count == tick + 1U);
        CHECK(g_scheduler_ticks == tick + 1U);
    }

    // Retain equal-priority preference during an unfinished quantum, then
    // rotate on expiry. A stronger runnable task still preempts that preference.
    prepare_processes();
    for (size_t slot = 0U; slot < 2U; ++slot) {
        apply_scheduler_profile(&g_processes[slot], 48U, 48U, 2U, 0U);
    }
    CHECK(capture_timer_irq() == kReturnedToUser);
    CHECK(g_current_process == &g_processes[0]);
    CHECK(g_processes[0].ticks_remaining == 1U);
    CHECK(capture_timer_irq() == kReturnedToUser);
    CHECK(g_current_process == &g_processes[1]);
    g_processes[0].priority = 16U;
    CHECK(capture_timer_irq() == kReturnedToUser);
    CHECK(g_current_process == &g_processes[0]);

    // Console readiness retains its wake and priority semantics; a lone
    // runnable task keeps the CPU when the other task blocks again.
    prepare_processes();
    apply_scheduler_profile(&g_processes[0], 48U, 48U, 2U, 0U);
    g_processes[1].state = ProcessState::Waiting;
    g_processes[1].wait_reason = WaitReason::ConsoleInput;
    input_ready = true;
    CHECK(capture_timer_irq() == kReturnedToUser);
    CHECK(g_current_process == &g_processes[1]);
    CHECK(g_processes[1].state == ProcessState::Runnable);
    CHECK(g_processes[1].wait_reason == WaitReason::None);
    input_ready = false;
    g_processes[0].state = ProcessState::Waiting;
    g_processes[1].ticks_remaining = 1U;
    CHECK(capture_timer_irq() == kReturnedToUser);
    CHECK(g_current_process == &g_processes[1]);

    std::puts("i486 signal, syscall-return, and timer fairness checks passed");
    return 0;
}
