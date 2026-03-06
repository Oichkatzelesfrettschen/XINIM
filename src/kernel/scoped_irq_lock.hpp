#pragma once
/**
 * @file scoped_irq_lock.hpp
 * @brief RAII interrupt disable/restore guard for x86_64.
 *
 * WHY: Scheduler and lock operations must be interrupt-safe. Bare cli/sti
 *      pairs are error-prone; RAII ensures correct restore on all exit paths.
 *
 * HOW: Constructor saves RFLAGS via pushfq then clears IF via cli.
 *      Destructor restores the saved RFLAGS via push/popfq, preserving
 *      the previous interrupt state (works correctly when nested).
 */

#include <cstdint>

namespace xinim::kernel {

class ScopedIrqLock {
  public:
    ScopedIrqLock() noexcept {
#ifdef XINIM_ARCH_X86_64
        asm volatile(
            "pushfq\n\t"
            "cli\n\t"
            "pop %0"
            : "=r"(saved_flags_)
            : : "memory"
        );
#endif
    }

    ~ScopedIrqLock() {
#ifdef XINIM_ARCH_X86_64
        asm volatile(
            "push %0\n\t"
            "popfq"
            : : "r"(saved_flags_)
            : "memory"
        );
#endif
    }

    ScopedIrqLock(const ScopedIrqLock&) = delete;
    ScopedIrqLock& operator=(const ScopedIrqLock&) = delete;

  private:
    uint64_t saved_flags_{0};
};

} // namespace xinim::kernel
