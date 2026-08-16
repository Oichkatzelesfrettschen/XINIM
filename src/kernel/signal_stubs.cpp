#include "pcb.hpp"
#include "signal.hpp"
#include "unified_scheduler.hpp"
#ifdef XINIM_ARCH_X86_64
#include "arch/x86_64/process_syscalls.hpp"
#endif

#include <cerrno>
#include <cstddef>
#include <cstring>

extern "C" void *malloc(std::size_t size);
extern "C" void free(void *pointer);

namespace xinim::kernel {
    namespace {

        [[nodiscard]] constexpr uint64_t signal_bit(int signum) noexcept {
            return uint64_t{1} << static_cast<unsigned int>(signum);
        }

        [[nodiscard]] constexpr uint64_t unmaskable_signals() noexcept {
            return signal_bit(SIGKILL) | signal_bit(SIGSTOP);
        }

    } // namespace

    SignalDefaultAction get_default_action(int signum) {
        switch (signum) {
        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
            return SignalDefaultAction::IGNORE;
        case SIGCONT:
            return SignalDefaultAction::CONT;
        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            return SignalDefaultAction::STOP;
        case SIGQUIT:
        case SIGILL:
        case SIGTRAP:
        case SIGABRT:
        case SIGBUS:
        case SIGFPE:
        case SIGSEGV:
        case SIGXCPU:
        case SIGXFSZ:
        case SIGSYS:
            return SignalDefaultAction::CORE;
        default:
            return SignalDefaultAction::TERM;
        }
    }

    bool init_signal_state(ProcessControlBlock *pcb) noexcept {
        if (pcb == nullptr) {
            return false;
        }
        auto *state = static_cast<SignalState *>(malloc(sizeof(SignalState)));
        if (state == nullptr) {
            pcb->signal_state = nullptr;
            return false;
        }
        std::memset(state, 0, sizeof(*state));
        pcb->signal_state = state;
        return true;
    }

    void destroy_signal_state(ProcessControlBlock *pcb) noexcept {
        if (pcb == nullptr) {
            return;
        }
        free(pcb->signal_state);
        pcb->signal_state = nullptr;
    }

    bool copy_signal_state(const ProcessControlBlock *parent, ProcessControlBlock *child) noexcept {
        if (parent == nullptr || child == nullptr || parent->signal_state == nullptr ||
            child->signal_state == nullptr) {
            return false;
        }
        *child->signal_state = *parent->signal_state;
        child->signal_state->pending = 0U;
        child->signal_state->in_handler = false;
        child->signal_state->saved_mask = 0U;
        child->signal_state->suspend_active = false;
        child->signal_state->suspend_saved_mask = 0U;
        return true;
    }

    void reset_signal_handlers(ProcessControlBlock *pcb) noexcept {
        if (pcb == nullptr || pcb->signal_state == nullptr) {
            return;
        }
        for (int signum = 1; signum < NSIG; ++signum) {
            SignalHandler &handler = pcb->signal_state->handlers[signum];
            if (handler.handler != SIG_IGN) {
                handler = {};
            }
        }
        pcb->signal_state->in_handler = false;
        pcb->signal_state->saved_mask = 0U;
        pcb->signal_state->suspend_active = false;
        pcb->signal_state->suspend_saved_mask = 0U;
    }

    int send_signal(ProcessControlBlock *pcb, int signum) noexcept {
        if (pcb == nullptr || pcb->signal_state == nullptr) {
            return -ESRCH;
        }
        if (signum <= 0 || signum >= NSIG) {
            return -EINVAL;
        }

        SignalState &state = *pcb->signal_state;
        if (signum == SIGCONT) {
            state.pending &= ~(signal_bit(SIGSTOP) | signal_bit(SIGTSTP) | signal_bit(SIGTTIN) |
                               signal_bit(SIGTTOU));
            if (pcb->state == ProcessState::STOPPED) {
#ifdef XINIM_ARCH_X86_64
                x86_64::process_continue(*pcb);
#else
                g_unified_scheduler.enqueue(pcb);
#endif
            }
        } else if (signum == SIGKILL && pcb->state == ProcessState::STOPPED) {
            g_unified_scheduler.enqueue(pcb);
        } else if (signum == SIGSTOP || signum == SIGTSTP || signum == SIGTTIN ||
                   signum == SIGTTOU) {
            state.pending &= ~signal_bit(SIGCONT);
        }

        state.pending |= signal_bit(signum);
        const SignalHandler &disposition = state.handlers[signum];
        const bool ignored = disposition.handler == SIG_IGN ||
                             (disposition.handler == SIG_DFL &&
                              get_default_action(signum) == SignalDefaultAction::IGNORE);
        if (pcb->state == ProcessState::BLOCKED && !ignored) {
#ifdef XINIM_ARCH_X86_64
            x86_64::process_interrupt_sleep(*pcb);
            x86_64::process_interrupt_select(*pcb);
#endif
            pcb->context.rax = static_cast<uint64_t>(-EINTR);
            g_unified_scheduler.unblock(pcb->pid);
        }
        return 0;
    }

    int take_pending_signal(ProcessControlBlock *pcb) noexcept {
        if (pcb == nullptr || pcb->signal_state == nullptr) {
            return 0;
        }
        SignalState &state = *pcb->signal_state;
        const uint64_t deliverable = state.pending & (~state.blocked | unmaskable_signals());
        if (deliverable == 0U) {
            return 0;
        }
        for (int signum = 1; signum < NSIG; ++signum) {
            const uint64_t bit = signal_bit(signum);
            if ((deliverable & bit) != 0U) {
                state.pending &= ~bit;
                return signum;
            }
        }
        return 0;
    }

} // namespace xinim::kernel
