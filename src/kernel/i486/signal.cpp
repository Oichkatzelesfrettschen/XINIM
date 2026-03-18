#include "signal.hpp"

#include "kutil.hpp"
#include "process.hpp"
#include "sched.hpp"

namespace xinim::i486::ring3 {

void init_signal_state(Process* process) noexcept {
    if (process == nullptr) {
        return;
    }
    zero_region(reinterpret_cast<uint8_t*>(&process->signals),
                static_cast<uint32_t>(sizeof(process->signals)));
    process->signals.handlers[kSigChld].handler = kSigIgn;
}

bool is_default_terminate(uint32_t signum) noexcept {
    switch (signum) {
    case kSigHup: case kSigInt: case kSigQuit: case kSigIll:
    case kSigAbrt: case kSigKill: case kSigSegv: case kSigPipe:
    case kSigAlrm: case kSigTerm:
        return true;
    default:
        return false;
    }
}

bool is_default_ignore(uint32_t signum) noexcept {
    switch (signum) {
    case kSigChld: case kSigCont:
        return true;
    default:
        return false;
    }
}

void send_signal_to_process(Process* target, uint32_t signum) noexcept {
    if (target == nullptr || signum == 0U || signum >= kMaxSignals) {
        return;
    }
    target->signals.pending |= (1U << signum);
    if (target->state == ProcessState::Waiting) {
        target->state = ProcessState::Runnable;
        target->wait_reason = WaitReason::None;
        target->wake_tick = 0U;
    }
}

bool deliver_one_signal(Process* process) noexcept {
    if (process == nullptr || process->signals.in_handler) {
        return false;
    }

    const uint32_t deliverable = process->signals.pending & ~process->signals.blocked;
    if (deliverable == 0U) {
        return false;
    }

    uint32_t signum = 0U;
    for (uint32_t i = 1U; i < kMaxSignals; ++i) {
        if ((deliverable & (1U << i)) != 0U) {
            signum = i;
            break;
        }
    }
    if (signum == 0U) {
        return false;
    }

    process->signals.pending &= ~(1U << signum);

    const SignalHandler32& handler = process->signals.handlers[signum];

    if (handler.handler == kSigIgn) {
        return false;
    }

    if (handler.handler == kSigDfl) {
        if (is_default_ignore(signum)) {
            return false;
        }
        if (signum == kSigStop || signum == kSigTstp) {
            process->state = ProcessState::Stopped;
            process->exit_status = (signum << 8U) | 0x7FU;
            Process* parent = find_process(process->ppid);
            if (parent != nullptr) {
                send_signal_to_process(parent, kSigChld);
                if (parent->state == ProcessState::Waiting) {
                    resume_waiting_parent(parent);
                }
            }
            return true;
        }
        if (signum == kSigCont) {
            if (process->state == ProcessState::Stopped) {
                process->state = ProcessState::Runnable;
            }
            return false;
        }
        if (is_default_terminate(signum)) {
            process->exit_status = 128U + signum;
            process->state = ProcessState::Exited;
            return true;
        }
        return false;
    }

    // User handler -- push signal frame onto user stack
    const uint32_t frame_size = static_cast<uint32_t>(sizeof(SignalFrame32));
    uint32_t new_esp = process->context.esp - frame_size;
    new_esp = align_down(new_esp, 4U);

    uint8_t* frame_dest = nullptr;
    if (!translate_user_region(process, new_esp, frame_size, &frame_dest)) {
        process->exit_status = 128U + signum;
        process->state = ProcessState::Exited;
        return true;
    }

    auto* sig_frame = reinterpret_cast<SignalFrame32*>(frame_dest);

    {
        auto* code = reinterpret_cast<uint8_t*>(sig_frame->sigreturn_trampoline);
        code[0] = 0xB8U;
        code[1] = static_cast<uint8_t>(SYS_rt_sigreturn & 0xFFU);
        code[2] = static_cast<uint8_t>((SYS_rt_sigreturn >> 8U) & 0xFFU);
        code[3] = 0U;
        code[4] = 0U;
        code[5] = 0xCDU;
        code[6] = 0x80U;
        code[7] = 0x90U;
    }

    sig_frame->signum = signum;
    sig_frame->saved_context = process->context;
    sig_frame->saved_mask = process->signals.blocked;

    process->signals.saved_mask = process->signals.blocked;
    process->signals.blocked |= handler.mask | (1U << signum);
    if ((handler.flags & kSaNodefer) != 0U) {
        process->signals.blocked &= ~(1U << signum);
    }
    process->signals.in_handler = true;

    if ((handler.flags & kSaResethand) != 0U) {
        process->signals.handlers[signum].handler = kSigDfl;
        process->signals.handlers[signum].flags = 0U;
    }

    const uint32_t trampoline_addr = new_esp;
    new_esp -= 4U;
    static_cast<void>(write_user_u32(process, new_esp, signum));
    new_esp -= 4U;
    static_cast<void>(write_user_u32(process, new_esp, trampoline_addr));

    process->context.eip = handler.handler;
    process->context.esp = new_esp;

    return true;
}

uint32_t sys_sigaction_impl(Process* process, RegisterFrame* frame) noexcept {
    const uint32_t signum = frame->ebx;
    const uint32_t new_act_addr = frame->ecx;
    const uint32_t old_act_addr = frame->edx;

    if (signum == 0U || signum >= kMaxSignals || signum == kSigKill || signum == kSigStop) {
        return kErrnoInvalid;
    }

    if (old_act_addr != 0U) {
        SigAction32User old_act{};
        old_act.sa_handler = process->signals.handlers[signum].handler;
        old_act.sa_flags = process->signals.handlers[signum].flags;
        old_act.sa_restorer = process->signals.handlers[signum].restorer;
        old_act.sa_mask = process->signals.handlers[signum].mask;
        if (!write_user_bytes(process, old_act_addr, &old_act,
                              static_cast<uint32_t>(sizeof(old_act)))) {
            return kErrnoFault;
        }
    }

    if (new_act_addr != 0U) {
        uint8_t* raw = nullptr;
        if (!translate_user_region(process, new_act_addr,
                                   static_cast<uint32_t>(sizeof(SigAction32User)), &raw)) {
            return kErrnoFault;
        }
        const auto* new_act = reinterpret_cast<const SigAction32User*>(raw);
        process->signals.handlers[signum].handler = new_act->sa_handler;
        process->signals.handlers[signum].flags = new_act->sa_flags;
        process->signals.handlers[signum].restorer = new_act->sa_restorer;
        process->signals.handlers[signum].mask = new_act->sa_mask;
    }

    return 0U;
}

uint32_t sys_kill_impl(Process* process, RegisterFrame* frame) noexcept {
    (void)process;
    const int32_t target_pid = static_cast<int32_t>(frame->ebx);
    const uint32_t signum = frame->ecx;

    if (signum >= kMaxSignals) {
        return kErrnoInvalid;
    }

    if (signum == 0U) {
        if (target_pid > 0) {
            return find_process(static_cast<uint32_t>(target_pid)) != nullptr ? 0U : kErrnoNoSys;
        }
        return 0U;
    }

    if (target_pid > 0) {
        Process* target = find_process(static_cast<uint32_t>(target_pid));
        if (target == nullptr) {
            return kErrnoNoSys;
        }
        send_signal_to_process(target, signum);
    } else if (target_pid == 0 || target_pid == -1) {
        for (auto& proc : g_processes) {
            if (proc.in_use && proc.state != ProcessState::Exited) {
                send_signal_to_process(&proc, signum);
            }
        }
    } else {
        Process* target = find_process(static_cast<uint32_t>(-target_pid));
        if (target == nullptr) {
            return kErrnoNoSys;
        }
        send_signal_to_process(target, signum);
    }

    return 0U;
}

[[noreturn]] void sys_rt_sigreturn_impl(Process* process, RegisterFrame* frame) noexcept {
    (void)frame;
    const uint32_t frame_addr = process->context.esp + 4U;
    uint8_t* raw = nullptr;
    if (!translate_user_region(process, frame_addr,
                               static_cast<uint32_t>(sizeof(SignalFrame32)), &raw)) {
        process->exit_status = 128U + kSigSegv;
        process->state = ProcessState::Exited;
        dispatch_next_runnable("sigreturn failed to read signal frame");
        __builtin_unreachable();
    }

    const auto* sig_frame = reinterpret_cast<const SignalFrame32*>(raw);

    process->context = sig_frame->saved_context;
    process->signals.blocked = sig_frame->saved_mask;
    process->signals.in_handler = false;

    // Sanitize security-sensitive fields: prevent IOPL escalation via crafted
    // signal frame. Only allow user-controllable arithmetic/status flags.
    constexpr uint32_t kSafeEflagsMask = 0x00000CD5U; // CF,PF,AF,ZF,SF,OF,DF
    process->context.eflags = (process->context.eflags & kSafeEflagsMask) | 0x202U;
    process->context.cs = kUserCodeSelector;
    process->context.ss = kUserDataSelector;
    process->context.ds = kUserDataSelector;
    process->context.es = kUserDataSelector;
    process->context.fs = kUserDataSelector;
    process->context.gs = kUserDataSelector;

    activate_process(process);
    i486_resume_user_context(&process->context);
    __builtin_unreachable();
}

uint32_t sys_signal_impl(Process* process, RegisterFrame* frame) noexcept {
    const uint32_t signum = frame->ebx;
    const uint32_t handler = frame->ecx;

    if (signum == 0U || signum >= kMaxSignals || signum == kSigKill || signum == kSigStop) {
        return kErrnoInvalid;
    }

    const uint32_t old_handler = process->signals.handlers[signum].handler;
    process->signals.handlers[signum].handler = handler;
    process->signals.handlers[signum].flags = kSaRestart;
    process->signals.handlers[signum].mask = 0U;
    process->signals.handlers[signum].restorer = 0U;
    return old_handler;
}

uint32_t sys_rt_sigprocmask_compat(Process* process, RegisterFrame* frame) noexcept {
    if (frame->edx != 0U) {
        const uint32_t words[32] = {};
        const uint32_t requested = frame->esi;
        const uint32_t available = static_cast<uint32_t>(sizeof(words));
        const uint32_t to_copy = requested < available ? requested : available;
        if (!write_user_bytes(process, frame->edx, words, to_copy)) {
            return kErrnoFault;
        }
    }
    return 0U;
}

} // namespace xinim::i486::ring3
