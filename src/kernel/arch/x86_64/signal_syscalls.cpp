#include "signal_syscalls.hpp"

#include "../../context.hpp"
#include "../../interrupts.hpp"
#include "../../pcb.hpp"
#include "../../scheduler.hpp"
#include "../../signal.hpp"
#include "../../uaccess.hpp"
#include "../../unified_scheduler.hpp"
#include "context_restore.hpp"
#include "elf64_user_image.hpp"
#include "process_syscalls.hpp"
#include "syscall_frame.hpp"
#include "syscall_init.hpp"
#include "tss.hpp"
#include "user_address_space.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <xinim/abi/x86_64_context_layout.h>
#include <xinim/abi/x86_segment_selectors.h>
#include <xinim/sys/syscalls.h>

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr uint64_t kSignalFrameMagic = 0x58494e494d534947ULL;
        constexpr uint64_t kSignalTrampolineAddress = kInitialUserStackTop;
        constexpr uint64_t kSignalTrampolineReturn = kSignalTrampolineAddress;
        constexpr uint64_t kSignalMaskUnblockable =
            (uint64_t{1} << SIGKILL) | (uint64_t{1} << SIGSTOP);
        constexpr uint64_t kSupportedActionFlags =
            SA_NOCLDSTOP | SA_RESTART | SA_NODEFER | SA_RESETHAND | 0x04000000U;
        constexpr uint64_t kUserRflagsMask = (uint64_t{1} << 0U) | (uint64_t{1} << 2U) |
                                             (uint64_t{1} << 4U) | (uint64_t{1} << 6U) |
                                             (uint64_t{1} << 7U) | (uint64_t{1} << 10U) |
                                             (uint64_t{1} << 11U) | (uint64_t{1} << 21U);
        constexpr uint64_t kRequiredUserRflags = (uint64_t{1} << 1U) | (uint64_t{1} << 9U);
        constexpr uint32_t kMxcsrValidBits = 0x0000ffbfU;

        struct alignas(16) SignalFrame64 {
            uint64_t magic;
            uint64_t saved_mask;
            CpuContext saved_context;
        };

        [[nodiscard]] constexpr bool valid_signal_number(int signum) noexcept {
            return signum > 0 && signum < NSIG;
        }

        [[nodiscard]] bool ensure_signal_trampoline(ProcessControlBlock &process) noexcept {
            const UserAddressSpace address_space{process.address_space_root};
            if (is_user_range_executable(address_space, kSignalTrampolineAddress, 1U)) {
                return true;
            }
            UserAddressSpace writable_space{process.address_space_root};
            if (!map_zeroed_user_page(writable_space, kSignalTrampolineAddress,
                                      UserPageFlags::Executable)) {
                return false;
            }
            const uint8_t trampoline[] = {
                0xb8U, static_cast<uint8_t>(SYS_rt_sigreturn),
                0x00U, 0x00U,
                0x00U, 0x0fU,
                0x05U, 0x0fU,
                0x0bU,
            };
            return copy_to_user_address_space(writable_space, kSignalTrampolineAddress, trampoline,
                                              sizeof(trampoline));
        }

        void capture_syscall_context(ProcessControlBlock &process, const SyscallFrame &frame,
                                     uint64_t result, CpuContext &context) noexcept {
            context.r15 = frame.r15;
            context.r14 = frame.r14;
            context.r13 = frame.r13;
            context.r12 = frame.r12;
            context.r11 = 0U;
            context.r10 = frame.r10;
            context.r9 = frame.r9;
            context.r8 = frame.r8;
            context.rbp = frame.rbp;
            context.rdi = frame.rdi;
            context.rsi = frame.rsi;
            context.rdx = frame.rdx;
            context.rcx = 0U;
            context.rbx = frame.rbx;
            context.rax = result;
            context.gs = XINIM_X86_USER_DS_SELECTOR;
            context.fs = XINIM_X86_USER_DS_SELECTOR;
            context.es = XINIM_X86_USER_DS_SELECTOR;
            context.ds = XINIM_X86_USER_DS_SELECTOR;
            context.rip = frame.rip;
            context.cs = XINIM_X86_USER_CS_SELECTOR;
            context.rflags = frame.rflags;
            context.rsp = frame.rsp;
            context.ss = XINIM_X86_USER_DS_SELECTOR;
            context.cr3 = process.address_space_root;
            const auto *fxsave_image =
                reinterpret_cast<const uint8_t *>(&frame) - XINIM_X86_64_FXSAVE_SIZE;
            std::memcpy(context.fxsave_area, fxsave_image, sizeof(context.fxsave_area));
        }

        void apply_syscall_context(const CpuContext &context, SyscallFrame &frame) noexcept {
            frame.r15 = context.r15;
            frame.r14 = context.r14;
            frame.r13 = context.r13;
            frame.r12 = context.r12;
            frame.r10 = context.r10;
            frame.r9 = context.r9;
            frame.r8 = context.r8;
            frame.rbp = context.rbp;
            frame.rdi = context.rdi;
            frame.rsi = context.rsi;
            frame.rdx = context.rdx;
            frame.rbx = context.rbx;
            frame.rip = context.rip;
            frame.cs = context.cs;
            frame.rflags = context.rflags;
            frame.rsp = context.rsp;
            frame.ss = context.ss;
        }

        void capture_interrupt_context(ProcessControlBlock &process,
                                       const X86_64InterruptFrame &frame, const void *fxsave_image,
                                       CpuContext &context) noexcept {
            context.r15 = frame.r15;
            context.r14 = frame.r14;
            context.r13 = frame.r13;
            context.r12 = frame.r12;
            context.r11 = frame.r11;
            context.r10 = frame.r10;
            context.r9 = frame.r9;
            context.r8 = frame.r8;
            context.rbp = frame.rbp;
            context.rdi = frame.rdi;
            context.rsi = frame.rsi;
            context.rdx = frame.rdx;
            context.rcx = frame.rcx;
            context.rbx = frame.rbx;
            context.rax = frame.rax;
            context.gs = frame.gs;
            context.fs = frame.fs;
            context.es = frame.es;
            context.ds = frame.ds;
            context.rip = frame.rip;
            context.cs = frame.cs;
            context.rflags = frame.rflags;
            context.rsp = frame.rsp;
            context.ss = frame.ss;
            context.cr3 = process.address_space_root;
            std::memcpy(context.fxsave_area, fxsave_image, sizeof(context.fxsave_area));
        }

        void apply_interrupt_context(const CpuContext &context, X86_64InterruptFrame &frame,
                                     void *fxsave_image) noexcept {
            frame.r15 = context.r15;
            frame.r14 = context.r14;
            frame.r13 = context.r13;
            frame.r12 = context.r12;
            frame.r11 = context.r11;
            frame.r10 = context.r10;
            frame.r9 = context.r9;
            frame.r8 = context.r8;
            frame.rbp = context.rbp;
            frame.rdi = context.rdi;
            frame.rsi = context.rsi;
            frame.rdx = context.rdx;
            frame.rcx = context.rcx;
            frame.rbx = context.rbx;
            frame.rax = context.rax;
            frame.rip = context.rip;
            frame.cs = context.cs;
            frame.rflags = context.rflags;
            frame.rsp = context.rsp;
            frame.ss = context.ss;
            std::memcpy(fxsave_image, context.fxsave_area, sizeof(context.fxsave_area));
        }

        [[nodiscard]] bool valid_restored_context(ProcessControlBlock &process,
                                                  CpuContext &context) noexcept {
            const UserAddressSpace address_space{process.address_space_root};
            if (!is_user_range_executable(address_space, context.rip, 1U) ||
                !is_user_range_mapped(address_space, context.rsp, 1U, true)) {
                return false;
            }
            context.cs = XINIM_X86_USER_CS_SELECTOR;
            context.ss = XINIM_X86_USER_DS_SELECTOR;
            context.ds = XINIM_X86_USER_DS_SELECTOR;
            context.es = XINIM_X86_USER_DS_SELECTOR;
            context.fs = XINIM_X86_USER_DS_SELECTOR;
            context.gs = XINIM_X86_USER_DS_SELECTOR;
            context.rflags = (context.rflags & kUserRflagsMask) | kRequiredUserRflags;
            context.cr3 = process.address_space_root;
            uint32_t mxcsr = 0U;
            std::memcpy(&mxcsr, context.fxsave_area + XINIM_X86_64_FXSAVE_MXCSR_OFFSET,
                        sizeof(mxcsr));
            mxcsr &= kMxcsrValidBits;
            std::memcpy(context.fxsave_area + XINIM_X86_64_FXSAVE_MXCSR_OFFSET, &mxcsr,
                        sizeof(mxcsr));
            return true;
        }

        [[nodiscard]] bool install_user_handler(ProcessControlBlock &process, CpuContext &context,
                                                int signum, SignalHandler &handler) noexcept {
            const UserAddressSpace address_space{process.address_space_root};
            if (!is_user_range_executable(address_space, handler.handler, 1U) ||
                !ensure_signal_trampoline(process) ||
                context.rsp < sizeof(SignalFrame64) + sizeof(uint64_t)) {
                return false;
            }

            const uint64_t frame_address = (context.rsp - sizeof(SignalFrame64)) & ~uint64_t{0x0fU};
            const uint64_t handler_stack = frame_address - sizeof(uint64_t);
            if (!is_user_range_mapped(address_space, handler_stack,
                                      sizeof(SignalFrame64) + sizeof(uint64_t), true)) {
                return false;
            }

            SignalFrame64 signal_frame{};
            signal_frame.magic = kSignalFrameMagic;
            signal_frame.saved_mask = process.signal_state->suspend_active
                                          ? process.signal_state->suspend_saved_mask
                                          : process.signal_state->blocked;
            signal_frame.saved_context = context;
            if (!copy_to_user_address_space(address_space, frame_address, &signal_frame,
                                            sizeof(signal_frame)) ||
                !copy_to_user_address_space(address_space, handler_stack, &kSignalTrampolineReturn,
                                            sizeof(kSignalTrampolineReturn))) {
                return false;
            }

            const uint64_t handler_address = handler.handler;
            SignalState &state = *process.signal_state;
            state.blocked |= handler.mask;
            if ((handler.flags & SA_NODEFER) == 0U) {
                state.blocked |= uint64_t{1} << static_cast<unsigned int>(signum);
            }
            state.blocked &= ~kSignalMaskUnblockable;
            state.in_handler = true;
            state.saved_mask = signal_frame.saved_mask;
            state.suspend_active = false;
            state.suspend_saved_mask = 0U;
            if ((handler.flags & SA_RESETHAND) != 0U) {
                handler = {};
            }

            context.rip = handler_address;
            context.rsp = handler_stack;
            context.rdi = static_cast<uint64_t>(signum);
            context.rsi = 0U;
            context.rdx = 0U;
            context.rflags = (context.rflags & kUserRflagsMask) | kRequiredUserRflags;
            return true;
        }

    } // namespace

    int64_t process_kill(int pid, int signum) noexcept {
        ProcessControlBlock *caller = get_current_process();
        if (caller == nullptr || signum < 0 || signum >= NSIG) {
            return -EINVAL;
        }
        int delivered = 0;
        for (int candidate_pid = 1; candidate_pid < MAX_PROCESSES; ++candidate_pid) {
            ProcessControlBlock *target = find_process_by_pid(candidate_pid);
            if (target == nullptr || target->state == ProcessState::DEAD ||
                target->state == ProcessState::ZOMBIE) {
                continue;
            }
            const bool selected =
                (pid > 0 && target->pid == pid) || (pid == 0 && target->pgid == caller->pgid) ||
                (pid == -1 && target->pid != 1) || (pid < -1 && target->pgid == -pid);
            if (!selected) {
                continue;
            }
            ++delivered;
            if (signum != 0) {
                const int result = send_signal(target, signum);
                if (result != 0) {
                    return result;
                }
            }
        }
        return delivered != 0 ? 0 : -ESRCH;
    }

    int64_t process_signal(int signum, uint64_t handler_address) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr || process->signal_state == nullptr ||
            !valid_signal_number(signum) || signum == SIGKILL || signum == SIGSTOP) {
            return -EINVAL;
        }
        SignalHandler &handler = process->signal_state->handlers[signum];
        const uint64_t previous = handler.handler;
        handler = {};
        handler.handler = handler_address;
        handler.flags = SA_RESTART;
        return static_cast<int64_t>(previous);
    }

    int64_t process_sigaction(int signum, uintptr_t action, uintptr_t old_action) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr || process->signal_state == nullptr ||
            !valid_signal_number(signum) || signum == SIGKILL || signum == SIGSTOP) {
            return -EINVAL;
        }
        SignalHandler &installed = process->signal_state->handlers[signum];
        if (old_action != 0U) {
            sigaction_user old_user{};
            old_user.sa_handler = installed.handler;
            old_user.sa_flags = installed.flags;
            old_user.sa_restorer = installed.restorer;
            old_user.sa_mask = installed.mask;
            if (copy_to_user(old_action, &old_user, sizeof(old_user)) != 0) {
                return -EFAULT;
            }
        }
        if (action != 0U) {
            sigaction_user new_user{};
            if (copy_from_user(&new_user, action, sizeof(new_user)) != 0) {
                return -EFAULT;
            }
            if ((new_user.sa_flags & ~kSupportedActionFlags) != 0U) {
                return -EINVAL;
            }
            if (new_user.sa_handler > SIG_IGN) {
                const UserAddressSpace address_space{process->address_space_root};
                if (!is_user_range_executable(address_space, new_user.sa_handler, 1U)) {
                    return -EFAULT;
                }
            }
            installed.handler = new_user.sa_handler;
            installed.flags = new_user.sa_flags;
            installed.restorer = new_user.sa_restorer;
            installed.mask = new_user.sa_mask & ~kSignalMaskUnblockable;
        }
        return 0;
    }

    int64_t process_sigprocmask(int how, uintptr_t set, uintptr_t old_set,
                                size_t signal_set_size) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr || process->signal_state == nullptr) {
            return -ESRCH;
        }
        if (signal_set_size != sizeof(uint64_t)) {
            return -EINVAL;
        }
        SignalState &state = *process->signal_state;
        if (old_set != 0U && copy_to_user(old_set, &state.blocked, sizeof(state.blocked)) != 0) {
            return -EFAULT;
        }
        if (set == 0U) {
            return 0;
        }
        uint64_t requested = 0U;
        if (copy_from_user(&requested, set, sizeof(requested)) != 0) {
            return -EFAULT;
        }
        requested &= ~kSignalMaskUnblockable;
        switch (how) {
        case SIG_BLOCK:
            state.blocked |= requested;
            break;
        case SIG_UNBLOCK:
            state.blocked &= ~requested;
            break;
        case SIG_SETMASK:
            state.blocked = requested;
            break;
        default:
            return -EINVAL;
        }
        return 0;
    }

    int64_t process_sigsuspend(uintptr_t set, size_t signal_set_size) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr || process->signal_state == nullptr ||
            signal_set_size != sizeof(uint64_t)) {
            return -EINVAL;
        }
        uint64_t requested_mask = 0U;
        if (copy_from_user(&requested_mask, set, sizeof(requested_mask)) != 0) {
            return -EFAULT;
        }
        SignalState &state = *process->signal_state;
        state.suspend_saved_mask = state.blocked;
        state.suspend_active = true;
        state.blocked = requested_mask & ~kSignalMaskUnblockable;
        if ((state.pending & ~state.blocked) != 0U) {
            return -EINTR;
        }
        process_block_for_signal();
    }

    [[noreturn]] void process_sigreturn() noexcept {
        ProcessControlBlock *process = get_current_process();
        const SyscallFrame *syscall_frame = active_syscall_frame();
        if (process == nullptr || process->signal_state == nullptr || syscall_frame == nullptr) {
            process_exit_for_signal(SIGSEGV);
        }
        SignalFrame64 signal_frame{};
        if (copy_from_user(&signal_frame, syscall_frame->rsp, sizeof(signal_frame)) != 0 ||
            signal_frame.magic != kSignalFrameMagic ||
            !valid_restored_context(*process, signal_frame.saved_context)) {
            process_exit_for_signal(SIGSEGV);
        }
        process->signal_state->blocked = signal_frame.saved_mask & ~kSignalMaskUnblockable;
        process->signal_state->in_handler = false;
        process->signal_state->saved_mask = 0U;
        process->context = signal_frame.saved_context;
        set_kernel_stack(process->kernel_rsp);
        set_syscall_kernel_stack(process->kernel_rsp);
        load_context_ring3(&process->context);
    }

    void deliver_signals_to_context(ProcessControlBlock &process, CpuContext &context) noexcept {
        for (;;) {
            const int signum = take_pending_signal(&process);
            if (signum == 0) {
                return;
            }
            SignalHandler &handler = process.signal_state->handlers[signum];
            if (handler.handler == SIG_IGN ||
                (handler.handler == SIG_DFL &&
                 get_default_action(signum) == SignalDefaultAction::IGNORE)) {
                continue;
            }
            if (handler.handler == SIG_DFL) {
                switch (get_default_action(signum)) {
                case SignalDefaultAction::IGNORE:
                case SignalDefaultAction::CONT:
                    continue;
                case SignalDefaultAction::STOP:
                    process_stop(signum);
                case SignalDefaultAction::TERM:
                case SignalDefaultAction::CORE:
                    process_exit_for_signal(signum);
                }
            }
            if (!install_user_handler(process, context, signum, handler)) {
                process_exit_for_signal(SIGSEGV);
            }
            return;
        }
    }

    uint64_t deliver_signals_from_syscall(SyscallFrame &frame, uint64_t syscall_result) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr || process->signal_state == nullptr) {
            return syscall_result;
        }
        capture_syscall_context(*process, frame, syscall_result, process->context);
        deliver_signals_to_context(*process, process->context);
        apply_syscall_context(process->context, frame);
        return process->context.rax;
    }

    void deliver_signals_from_timer(ProcessControlBlock &process, X86_64InterruptFrame &frame,
                                    void *fxsave_image) noexcept {
        capture_interrupt_context(process, frame, fxsave_image, process.context);
        deliver_signals_to_context(process, process.context);
        apply_interrupt_context(process.context, frame, fxsave_image);
    }

} // namespace xinim::kernel::x86_64
