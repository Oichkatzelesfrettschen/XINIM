/**
 * @file timer.hpp
 * @brief Timer interrupt handling interface
 *
 * @ingroup kernel
 */

#ifndef XINIM_KERNEL_TIMER_HPP
#define XINIM_KERNEL_TIMER_HPP

#include "interrupts.hpp"

#include <cstdint>

// Forward declarations
namespace xinim::hal::x86_64 {
    class Lapic;
}

namespace xinim::kernel {

    inline constexpr uint64_t kSchedulerTicksPerSecond = 100U;

    /**
     * @brief Get timer tick count
     *
     * @return Number of timer interrupts since boot
     */
    uint64_t get_timer_ticks();

    /**
     * @brief Set LAPIC reference for timer EOI
     *
     * Must be called before timer interrupts are enabled.
     *
     * @param lapic Pointer to initialized LAPIC object
     */
    void set_timer_lapic(xinim::hal::x86_64::Lapic *lapic);

    void initialize_timer();

} // namespace xinim::kernel

/** @brief C++ timer callback entered through the assembly interrupt gate. */
extern "C" void timer_interrupt_c_handler(const xinim::kernel::X86_64InterruptFrame *frame,
                                          const void *fxsave_image) noexcept;

#endif /* XINIM_KERNEL_TIMER_HPP */
