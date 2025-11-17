/**
 * @file timer.hpp
 * @brief Timer interrupt handling interface
 *
 * @ingroup kernel
 */

#ifndef XINIM_KERNEL_TIMER_HPP
#define XINIM_KERNEL_TIMER_HPP

#include <cstdint>

namespace xinim::kernel {

/**
 * @brief Get timer tick count
 *
 * @return Number of timer interrupts since boot
 */
uint64_t get_timer_ticks();

} // namespace xinim::kernel

/**
 * @brief Initialize timer subsystem
 *
 * @param frequency_hz Timer frequency in Hz (default: 100)
 */
extern "C" void initialize_timer(uint32_t frequency_hz = 100);

/**
 * @brief C++ timer interrupt handler (called from assembly)
 *
 * @param context Pointer to saved CPU context
 */
extern "C" void timer_interrupt_c_handler(xinim::kernel::CpuContext* context);

/**
 * @brief Handle unhandled interrupt
 */
extern "C" void handle_unhandled_interrupt();

#endif /* XINIM_KERNEL_TIMER_HPP */
