#include "runtime_syscalls.hpp"

#include "../../pcb.hpp"
#include "../../scheduler.hpp"
#include "../../timer.hpp"
#include "../../uaccess.hpp"
#include "../../unified_scheduler.hpp"
#include "elf64_user_image.hpp"
#include "process_syscalls.hpp"
#include "realtime_clock.hpp"
#include "userspace_abi.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr int kClockRealtime = 0;
        constexpr int kClockMonotonic = 1;
        constexpr int kResourceData = 2;
        constexpr int kResourceStack = 3;
        constexpr int kResourceCore = 4;
        constexpr int kResourceOpenFiles = 7;
        constexpr int kResourceAddressSpace = 9;
        constexpr int kResourceCount = 16;
        constexpr int kUsageSelf = 0;
        constexpr int kUsageChildren = -1;
        constexpr int kPriorityProcess = 0;

        [[nodiscard]] ProcessControlBlock *current_process() noexcept {
            return get_current_process();
        }

        [[nodiscard]] UserspaceTimeValue64 ticks_to_time_value(uint64_t ticks) noexcept {
            return {
                static_cast<int64_t>(ticks / kSchedulerTicksPerSecond),
                static_cast<int64_t>((ticks % kSchedulerTicksPerSecond) *
                                     (1000000U / kSchedulerTicksPerSecond)),
            };
        }

        [[nodiscard]] UserspaceResourceLimit64 resource_limit(const ProcessControlBlock &process,
                                                              int resource) noexcept {
            switch (resource) {
            case kResourceOpenFiles:
                return {process.descriptor_limit, MAX_FDS_PER_PROCESS};
            case kResourceStack:
                return {kInitialUserStackSize, kInitialUserStackSize};
            case kResourceCore:
                return {0U, 0U};
            case kResourceData:
            case kResourceAddressSpace:
                return {kInitialUserStackBottom, kInitialUserStackBottom};
            default:
                return {std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::max()};
            }
        }

        [[nodiscard]] bool assign_identity_component(int requested,
                                                     uint32_t &destination) noexcept {
            if (requested == -1) {
                return true;
            }
            if (requested < 0) {
                return false;
            }
            destination = static_cast<uint32_t>(requested);
            return true;
        }

    } // namespace

    int64_t process_time(uintptr_t output) noexcept {
        const int64_t now = static_cast<int64_t>(realtime_seconds());
        if (output != 0U && copy_to_user(output, &now, sizeof(now)) != 0) {
            return -EFAULT;
        }
        return now;
    }

    int64_t process_gettimeofday(uintptr_t time_value, uintptr_t timezone) noexcept {
        if (time_value != 0U) {
            const UserspaceTimeValue64 value{
                static_cast<int64_t>(realtime_seconds()),
                static_cast<int64_t>(realtime_microseconds()),
            };
            if (copy_to_user(time_value, &value, sizeof(value)) != 0) {
                return -EFAULT;
            }
        }
        if (timezone != 0U) {
            const int32_t utc_timezone[2] = {0, 0};
            if (copy_to_user(timezone, utc_timezone, sizeof(utc_timezone)) != 0) {
                return -EFAULT;
            }
        }
        return 0;
    }

    int64_t process_clock_gettime(int clock_id, uintptr_t time_value) noexcept {
        if (time_value == 0U) {
            return -EFAULT;
        }
        UserspaceTimeSpec64 value{};
        if (clock_id == kClockRealtime) {
            value.seconds = static_cast<int64_t>(realtime_seconds());
            value.nanoseconds = static_cast<int64_t>(realtime_microseconds()) * 1000;
        } else if (clock_id == kClockMonotonic) {
            const uint64_t ticks = g_unified_scheduler.tick_count();
            value.seconds = static_cast<int64_t>(ticks / kSchedulerTicksPerSecond);
            value.nanoseconds = static_cast<int64_t>((ticks % kSchedulerTicksPerSecond) *
                                                     (1000000000ULL / kSchedulerTicksPerSecond));
        } else {
            return -EINVAL;
        }
        return copy_to_user(time_value, &value, sizeof(value)) == 0 ? 0 : -EFAULT;
    }

    int64_t process_nanosleep(uintptr_t request, uintptr_t remaining) noexcept {
        UserspaceTimeSpec64 requested{};
        if (copy_from_user(&requested, request, sizeof(requested)) != 0) {
            return -EFAULT;
        }
        if (requested.seconds < 0 || requested.nanoseconds < 0 ||
            requested.nanoseconds >= 1000000000LL) {
            return -EINVAL;
        }
        const uint64_t seconds = static_cast<uint64_t>(requested.seconds);
        if (seconds > std::numeric_limits<uint64_t>::max() / kSchedulerTicksPerSecond) {
            return -EINVAL;
        }
        uint64_t ticks = seconds * kSchedulerTicksPerSecond;
        ticks += (static_cast<uint64_t>(requested.nanoseconds) +
                  (1000000000ULL / kSchedulerTicksPerSecond) - 1U) /
                 (1000000000ULL / kSchedulerTicksPerSecond);
        if (ticks == 0U) {
            if (remaining != 0U) {
                const UserspaceTimeSpec64 zero{};
                if (copy_to_user(remaining, &zero, sizeof(zero)) != 0) {
                    return -EFAULT;
                }
            }
            return 0;
        }
        const uint64_t current_tick = g_unified_scheduler.tick_count();
        if (ticks > std::numeric_limits<uint64_t>::max() - current_tick) {
            return -EINVAL;
        }
        process_sleep_until(current_tick + ticks, remaining);
    }

    int64_t process_alarm(unsigned int seconds) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        const uint64_t current_tick = g_unified_scheduler.tick_count();
        uint64_t remaining = 0U;
        if (process->alarm_deadline_tick > current_tick) {
            const uint64_t remaining_ticks = process->alarm_deadline_tick - current_tick;
            remaining =
                (remaining_ticks + kSchedulerTicksPerSecond - 1U) / kSchedulerTicksPerSecond;
        }
        process->alarm_deadline_tick =
            seconds == 0U
                ? 0U
                : current_tick + static_cast<uint64_t>(seconds) * kSchedulerTicksPerSecond;
        return static_cast<int64_t>(remaining);
    }

    int64_t process_getrlimit(int resource, uintptr_t output) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        if (resource < 0 || resource >= kResourceCount) {
            return -EINVAL;
        }
        const UserspaceResourceLimit64 value = resource_limit(*process, resource);
        return copy_to_user(output, &value, sizeof(value)) == 0 ? 0 : -EFAULT;
    }

    int64_t process_setrlimit(int resource, uintptr_t input) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        if (resource < 0 || resource >= kResourceCount) {
            return -EINVAL;
        }
        UserspaceResourceLimit64 requested{};
        if (copy_from_user(&requested, input, sizeof(requested)) != 0) {
            return -EFAULT;
        }
        if (requested.current > requested.maximum) {
            return -EINVAL;
        }
        if (resource == kResourceOpenFiles) {
            if (requested.maximum > MAX_FDS_PER_PROCESS || requested.current < 3U) {
                return -EPERM;
            }
            process->descriptor_limit = requested.current;
            return 0;
        }
        const UserspaceResourceLimit64 actual = resource_limit(*process, resource);
        return requested.current == actual.current && requested.maximum == actual.maximum ? 0
                                                                                          : -EPERM;
    }

    int64_t process_getrusage(int who, uintptr_t output) noexcept {
        const ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        if (who != kUsageSelf && who != kUsageChildren) {
            return -EINVAL;
        }
        UserspaceResourceUsage64 usage{};
        usage.user_time =
            ticks_to_time_value(who == kUsageSelf ? process->total_ticks : process->children_ticks);
        return copy_to_user(output, &usage, sizeof(usage)) == 0 ? 0 : -EFAULT;
    }

    int64_t process_umask(uint32_t mask) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        const uint32_t previous = process->file_creation_mask;
        process->file_creation_mask = mask & 0777U;
        return previous;
    }

    int64_t process_getpriority(int which, int who) noexcept {
        ProcessControlBlock *caller = current_process();
        if (caller == nullptr || which != kPriorityProcess) {
            return -EINVAL;
        }
        ProcessControlBlock *target = who == 0 ? caller : find_process_by_pid(who);
        return target != nullptr ? 20 - target->nice_value : -ESRCH;
    }

    int64_t process_setpriority(int which, int who, int priority) noexcept {
        ProcessControlBlock *caller = current_process();
        if (caller == nullptr || which != kPriorityProcess) {
            return -EINVAL;
        }
        ProcessControlBlock *target = who == 0 ? caller : find_process_by_pid(who);
        if (target == nullptr) {
            return -ESRCH;
        }
        target->nice_value = std::clamp(priority, -20, 19);
        target->priority = static_cast<uint32_t>(
            std::clamp(static_cast<int>(PRIO_USER_NORM) + target->nice_value,
                       static_cast<int>(PRIO_USER_HIGH), static_cast<int>(PRIO_USER_LOW)));
        return 0;
    }

    int64_t process_setgroups(size_t count, uintptr_t groups) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        if (process->effective_user_id != 0U) {
            return -EPERM;
        }
        if (count > process->supplementary_groups.size()) {
            return -EINVAL;
        }
        if (count != 0U && copy_from_user(process->supplementary_groups.data(), groups,
                                          count * sizeof(uint32_t)) != 0) {
            return -EFAULT;
        }
        process->supplementary_group_count = count;
        return 0;
    }

    int64_t process_setresuid(int real, int effective, int saved) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        if (process->effective_user_id != 0U &&
            ((real != -1 && static_cast<uint32_t>(real) != process->real_user_id) ||
             (effective != -1 && static_cast<uint32_t>(effective) != process->effective_user_id) ||
             (saved != -1 && static_cast<uint32_t>(saved) != process->saved_user_id))) {
            return -EPERM;
        }
        return assign_identity_component(real, process->real_user_id) &&
                       assign_identity_component(effective, process->effective_user_id) &&
                       assign_identity_component(saved, process->saved_user_id)
                   ? 0
                   : -EINVAL;
    }

    int64_t process_setresgid(int real, int effective, int saved) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        if (process->effective_user_id != 0U &&
            ((real != -1 && static_cast<uint32_t>(real) != process->real_group_id) ||
             (effective != -1 && static_cast<uint32_t>(effective) != process->effective_group_id) ||
             (saved != -1 && static_cast<uint32_t>(saved) != process->saved_group_id))) {
            return -EPERM;
        }
        return assign_identity_component(real, process->real_group_id) &&
                       assign_identity_component(effective, process->effective_group_id) &&
                       assign_identity_component(saved, process->saved_group_id)
                   ? 0
                   : -EINVAL;
    }

} // namespace xinim::kernel::x86_64
