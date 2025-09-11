#pragma once

/**
 * @file scheduler.hpp
 * @brief Compatibility wrapper for the sched::Scheduler API.
 */

#include "schedule.hpp"

extern "C" void restart() noexcept;

namespace Scheduler {

/** Pick the next runnable task using the global scheduler. */
inline void pick() { (void)sched::scheduler.preempt(); }

/** Resume execution of the currently selected task. */
inline void restart() { ::restart(); }

} // namespace Scheduler
