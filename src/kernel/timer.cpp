/**
 * @file timer.cpp
 * @brief Timer interrupt handling (Bare-metal refactored)
 */

#include "timer.hpp"

#include "arch/x86_64/context_restore.hpp"
#include "arch/x86_64/signal_syscalls.hpp"
#include "arch/x86_64/syscall_init.hpp"
#include "arch/x86_64/tss.hpp"
#include "early/serial_16550.hpp"
#include "hal/x86_64/hal/apic.hpp"
#include "pcb.hpp"
#include "scheduler.hpp"
#include "unified_scheduler.hpp"

#include <cstring>
#include <stdint.h>
#include <xinim/abi/x86_segment_selectors.h>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

    static xinim::hal::x86_64::Lapic *g_timer_lapic = nullptr;

    void set_timer_lapic(xinim::hal::x86_64::Lapic *lapic) {
        g_timer_lapic = lapic;
    }

    void initialize_timer() {
        early_serial.write("[TIMER] Initialized\n");
    }

    uint64_t get_timer_ticks() {
        return g_unified_scheduler.tick_count();
    }

} // namespace xinim::kernel

namespace {

    [[nodiscard]] bool
    interrupted_user_mode(const xinim::kernel::X86_64InterruptFrame &frame) noexcept {
        return (frame.cs & 3U) == 3U;
    }

    void capture_interrupted_user(xinim::kernel::ProcessControlBlock &process,
                                  const xinim::kernel::X86_64InterruptFrame &frame,
                                  const void *fxsave_image) noexcept {
        xinim::kernel::CpuContext &context = process.context;
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

    [[noreturn]] void resume_process(xinim::kernel::ProcessControlBlock &process) noexcept {
        xinim::kernel::set_kernel_stack(process.kernel_rsp);
        xinim::kernel::set_syscall_kernel_stack(process.kernel_rsp);
        if (process.context.cs == XINIM_X86_USER_CS_SELECTOR) {
            load_context_ring3(&process.context);
        }
        load_context(&process.context);
    }

} // namespace

extern "C" void timer_interrupt_c_handler(const xinim::kernel::X86_64InterruptFrame *frame,
                                          const void *fxsave_image) noexcept {
    const bool can_preempt =
        frame != nullptr && fxsave_image != nullptr && interrupted_user_mode(*frame);
    xinim::kernel::ProcessControlBlock *interrupted = xinim::kernel::g_unified_scheduler.current();
    if (can_preempt && interrupted != nullptr) {
        capture_interrupted_user(*interrupted, *frame, fxsave_image);
    }
    xinim::kernel::g_unified_scheduler.timer_tick(can_preempt);
    xinim::kernel::ProcessControlBlock *selected = xinim::kernel::g_unified_scheduler.current();
    if (xinim::kernel::g_timer_lapic != nullptr) {
        xinim::kernel::g_timer_lapic->eoi();
    }
    if (!can_preempt || selected == nullptr) {
        return;
    }
    if (selected == interrupted) {
        xinim::kernel::x86_64::deliver_signals_from_timer(
            *selected, *const_cast<xinim::kernel::X86_64InterruptFrame *>(frame),
            const_cast<void *>(fxsave_image));
        return;
    }
    xinim::kernel::x86_64::deliver_signals_to_context(*selected, selected->context);
    resume_process(*selected);
}
