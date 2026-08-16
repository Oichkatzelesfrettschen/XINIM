/**
 * @file interrupts.hpp
 * @brief Interrupt handler declarations
 *
 * Declares assembly interrupt handlers and related C++ functions.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_INTERRUPTS_HPP
#define XINIM_KERNEL_INTERRUPTS_HPP

#include <cstddef>
#include <cstdint>
#include <xinim/abi/x86_64_context_layout.h>

namespace xinim::early {
    class Serial16550;
}

namespace xinim::hal::x86_64 {
    class Lapic;
}

namespace xinim::kernel {

    /** Register and hardware state built by the x86_64 interrupt wrappers. */
    struct X86_64InterruptFrame {
        uint64_t gs;
        uint64_t fs;
        uint64_t es;
        uint64_t ds;
        uint64_t r15;
        uint64_t r14;
        uint64_t r13;
        uint64_t r12;
        uint64_t r11;
        uint64_t r10;
        uint64_t r9;
        uint64_t r8;
        uint64_t rbp;
        uint64_t rdi;
        uint64_t rsi;
        uint64_t rdx;
        uint64_t rcx;
        uint64_t rbx;
        uint64_t rax;
        uint64_t rip;
        uint64_t cs;
        uint64_t rflags;
        uint64_t rsp;
        uint64_t ss;
    };

    static_assert(offsetof(X86_64InterruptFrame, gs) == XINIM_X86_64_INTERRUPT_GS);
    static_assert(offsetof(X86_64InterruptFrame, fs) == XINIM_X86_64_INTERRUPT_FS);
    static_assert(offsetof(X86_64InterruptFrame, es) == XINIM_X86_64_INTERRUPT_ES);
    static_assert(offsetof(X86_64InterruptFrame, ds) == XINIM_X86_64_INTERRUPT_DS);
    static_assert(offsetof(X86_64InterruptFrame, r15) == XINIM_X86_64_INTERRUPT_R15);
    static_assert(offsetof(X86_64InterruptFrame, r14) == XINIM_X86_64_INTERRUPT_R14);
    static_assert(offsetof(X86_64InterruptFrame, r13) == XINIM_X86_64_INTERRUPT_R13);
    static_assert(offsetof(X86_64InterruptFrame, r12) == XINIM_X86_64_INTERRUPT_R12);
    static_assert(offsetof(X86_64InterruptFrame, r11) == XINIM_X86_64_INTERRUPT_R11);
    static_assert(offsetof(X86_64InterruptFrame, r10) == XINIM_X86_64_INTERRUPT_R10);
    static_assert(offsetof(X86_64InterruptFrame, r9) == XINIM_X86_64_INTERRUPT_R9);
    static_assert(offsetof(X86_64InterruptFrame, r8) == XINIM_X86_64_INTERRUPT_R8);
    static_assert(offsetof(X86_64InterruptFrame, rbp) == XINIM_X86_64_INTERRUPT_RBP);
    static_assert(offsetof(X86_64InterruptFrame, rdi) == XINIM_X86_64_INTERRUPT_RDI);
    static_assert(offsetof(X86_64InterruptFrame, rsi) == XINIM_X86_64_INTERRUPT_RSI);
    static_assert(offsetof(X86_64InterruptFrame, rdx) == XINIM_X86_64_INTERRUPT_RDX);
    static_assert(offsetof(X86_64InterruptFrame, rcx) == XINIM_X86_64_INTERRUPT_RCX);
    static_assert(offsetof(X86_64InterruptFrame, rbx) == XINIM_X86_64_INTERRUPT_RBX);
    static_assert(offsetof(X86_64InterruptFrame, rax) == XINIM_X86_64_INTERRUPT_RAX);
    static_assert(offsetof(X86_64InterruptFrame, rip) == XINIM_X86_64_INTERRUPT_RIP);
    static_assert(offsetof(X86_64InterruptFrame, cs) == XINIM_X86_64_INTERRUPT_CS);
    static_assert(offsetof(X86_64InterruptFrame, rflags) == XINIM_X86_64_INTERRUPT_RFLAGS);
    static_assert(offsetof(X86_64InterruptFrame, rsp) == XINIM_X86_64_INTERRUPT_RSP);
    static_assert(offsetof(X86_64InterruptFrame, ss) == XINIM_X86_64_INTERRUPT_SS);
    static_assert(sizeof(X86_64InterruptFrame) == XINIM_X86_64_INTERRUPT_USER_SIZE);

    /** Register, vector, error, and hardware state built by exception wrappers. */
    struct X86_64ExceptionFrame {
        uint64_t gs;
        uint64_t fs;
        uint64_t es;
        uint64_t ds;
        uint64_t r15;
        uint64_t r14;
        uint64_t r13;
        uint64_t r12;
        uint64_t r11;
        uint64_t r10;
        uint64_t r9;
        uint64_t r8;
        uint64_t rbp;
        uint64_t rdi;
        uint64_t rsi;
        uint64_t rdx;
        uint64_t rcx;
        uint64_t rbx;
        uint64_t rax;
        uint64_t vector;
        uint64_t error_code;
        uint64_t rip;
        uint64_t cs;
        uint64_t rflags;
    };

    static_assert(offsetof(X86_64ExceptionFrame, gs) == XINIM_X86_64_EXCEPTION_GS);
    static_assert(offsetof(X86_64ExceptionFrame, fs) == XINIM_X86_64_EXCEPTION_FS);
    static_assert(offsetof(X86_64ExceptionFrame, es) == XINIM_X86_64_EXCEPTION_ES);
    static_assert(offsetof(X86_64ExceptionFrame, ds) == XINIM_X86_64_EXCEPTION_DS);
    static_assert(offsetof(X86_64ExceptionFrame, r15) == XINIM_X86_64_EXCEPTION_R15);
    static_assert(offsetof(X86_64ExceptionFrame, r14) == XINIM_X86_64_EXCEPTION_R14);
    static_assert(offsetof(X86_64ExceptionFrame, r13) == XINIM_X86_64_EXCEPTION_R13);
    static_assert(offsetof(X86_64ExceptionFrame, r12) == XINIM_X86_64_EXCEPTION_R12);
    static_assert(offsetof(X86_64ExceptionFrame, r11) == XINIM_X86_64_EXCEPTION_R11);
    static_assert(offsetof(X86_64ExceptionFrame, r10) == XINIM_X86_64_EXCEPTION_R10);
    static_assert(offsetof(X86_64ExceptionFrame, r9) == XINIM_X86_64_EXCEPTION_R9);
    static_assert(offsetof(X86_64ExceptionFrame, r8) == XINIM_X86_64_EXCEPTION_R8);
    static_assert(offsetof(X86_64ExceptionFrame, rbp) == XINIM_X86_64_EXCEPTION_RBP);
    static_assert(offsetof(X86_64ExceptionFrame, rdi) == XINIM_X86_64_EXCEPTION_RDI);
    static_assert(offsetof(X86_64ExceptionFrame, rsi) == XINIM_X86_64_EXCEPTION_RSI);
    static_assert(offsetof(X86_64ExceptionFrame, rdx) == XINIM_X86_64_EXCEPTION_RDX);
    static_assert(offsetof(X86_64ExceptionFrame, rcx) == XINIM_X86_64_EXCEPTION_RCX);
    static_assert(offsetof(X86_64ExceptionFrame, rbx) == XINIM_X86_64_EXCEPTION_RBX);
    static_assert(offsetof(X86_64ExceptionFrame, rax) == XINIM_X86_64_EXCEPTION_RAX);
    static_assert(offsetof(X86_64ExceptionFrame, vector) == XINIM_X86_64_EXCEPTION_VECTOR);
    static_assert(offsetof(X86_64ExceptionFrame, error_code) == XINIM_X86_64_EXCEPTION_ERROR_CODE);
    static_assert(offsetof(X86_64ExceptionFrame, rip) == XINIM_X86_64_EXCEPTION_RIP);
    static_assert(offsetof(X86_64ExceptionFrame, cs) == XINIM_X86_64_EXCEPTION_CS);
    static_assert(offsetof(X86_64ExceptionFrame, rflags) == XINIM_X86_64_EXCEPTION_RFLAGS);
    static_assert(sizeof(X86_64ExceptionFrame) == XINIM_X86_64_EXCEPTION_CORE_SIZE);

} // namespace xinim::kernel

// ============================================================================
// Assembly Interrupt Handlers
// ============================================================================

/**
 * @brief Timer interrupt handler (IRQ 0, Vector 32)
 *
 * Defined in arch/x86_64/interrupts.S
 * This is the low-level assembly entry point for timer interrupts.
 */
extern "C" void timer_interrupt_handler();

/**
 * @brief Spurious interrupt handler (Vector 255)
 *
 * Defined in arch/x86_64/interrupts.S
 * Handles spurious APIC interrupts.
 */
extern "C" void spurious_interrupt_handler();

/**
 * @brief Generic interrupt handler (for unhandled interrupts)
 *
 * Defined in arch/x86_64/interrupts.S
 * Used for debugging unhandled interrupts.
 */
extern "C" void generic_interrupt_handler();

extern "C" void keyboard_interrupt_handler();
extern "C" void com1_interrupt_handler();
extern "C" void com2_interrupt_handler();

/** @brief C++ keyboard callback entered through the assembly interrupt gate. */
extern "C" void
keyboard_interrupt_c_handler(const xinim::kernel::X86_64InterruptFrame *frame) noexcept;

/** @brief C++ COM1 callback entered through the assembly interrupt gate. */
extern "C" void com1_interrupt_c_handler(const xinim::kernel::X86_64InterruptFrame *frame) noexcept;

/** @brief C++ COM2 callback entered through the assembly interrupt gate. */
extern "C" void com2_interrupt_c_handler(const xinim::kernel::X86_64InterruptFrame *frame) noexcept;

extern "C" void handle_unhandled_irq(const xinim::kernel::X86_64InterruptFrame *frame) noexcept;
extern "C" [[noreturn]] void
handle_unhandled_exception(const xinim::kernel::X86_64ExceptionFrame *frame) noexcept;

#define XINIM_DECLARE_X86_64_EXCEPTION_GATE(vector) extern "C" void x86_64_exception_##vector();
XINIM_DECLARE_X86_64_EXCEPTION_GATE(0)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(1)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(2)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(3)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(4)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(5)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(6)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(7)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(8)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(9)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(10)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(11)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(12)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(13)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(14)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(15)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(16)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(17)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(18)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(19)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(20)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(21)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(22)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(23)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(24)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(25)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(26)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(27)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(28)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(29)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(30)
XINIM_DECLARE_X86_64_EXCEPTION_GATE(31)
#undef XINIM_DECLARE_X86_64_EXCEPTION_GATE

// ============================================================================
// Interrupt Initialization
// ============================================================================

/**
 * @brief Initialize the IDT and connect its assembly gates to device state.
 *
 * Must be called during kernel initialization before enabling interrupts.
 *
 * @param serial COM1 device used by interrupt callbacks and diagnostics
 * @param lapic Local APIC used to acknowledge external interrupts
 */
void interrupts_init(xinim::early::Serial16550 &serial, xinim::hal::x86_64::Lapic &lapic);

#endif /* XINIM_KERNEL_INTERRUPTS_HPP */
