#include "select_syscalls.hpp"

#include "../../fd_table.hpp"
#include "../../pcb.hpp"
#include "../../scheduler.hpp"
#include "../../timer.hpp"
#include "../../uaccess.hpp"
#include "../../unified_scheduler.hpp"
#include "bootfs_syscalls.hpp"
#include "process_syscalls.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr size_t kDescriptorSetWordCount = MAX_FDS_PER_PROCESS / 64U;

        struct DescriptorSet {
            uint64_t words[kDescriptorSetWordCount];
        };

        struct SelectTimeValue {
            int64_t seconds;
            int64_t microseconds;
        };

        static_assert(sizeof(DescriptorSet) == 128U);
        static_assert(sizeof(SelectTimeValue) == 16U);

        [[nodiscard]] bool descriptor_is_set(const DescriptorSet &set, int descriptor) noexcept {
            return (set.words[static_cast<size_t>(descriptor) / 64U] &
                    (uint64_t{1} << (static_cast<unsigned int>(descriptor) % 64U))) != 0U;
        }

        void set_descriptor(DescriptorSet &set, int descriptor) noexcept {
            set.words[static_cast<size_t>(descriptor) / 64U] |=
                uint64_t{1} << (static_cast<unsigned int>(descriptor) % 64U);
        }

        [[nodiscard]] bool copy_descriptor_set(uintptr_t address, DescriptorSet &set) noexcept {
            return address == 0U || copy_from_user(&set, address, sizeof(set)) == 0;
        }

        [[nodiscard]] bool write_descriptor_set(uintptr_t address,
                                                const DescriptorSet &set) noexcept {
            return address == 0U || copy_to_user(address, &set, sizeof(set)) == 0;
        }

        [[nodiscard]] bool any_set_contains(const DescriptorSet &read_set,
                                            const DescriptorSet &write_set,
                                            const DescriptorSet &exception_set,
                                            uintptr_t read_address, uintptr_t write_address,
                                            uintptr_t exception_address, int descriptor) noexcept {
            return (read_address != 0U && descriptor_is_set(read_set, descriptor)) ||
                   (write_address != 0U && descriptor_is_set(write_set, descriptor)) ||
                   (exception_address != 0U && descriptor_is_set(exception_set, descriptor));
        }

        void clear_select_state(ProcessControlBlock &process) noexcept {
            process.select_deadline_tick = 0U;
            process.select_timeout_address = 0U;
            process.select_active = false;
            process.wake_deadline_tick = 0U;
        }

        [[nodiscard]] bool write_remaining_timeout(uintptr_t address,
                                                   uint64_t deadline_tick) noexcept {
            if (address == 0U) {
                return true;
            }
            const uint64_t current_tick = g_unified_scheduler.tick_count();
            const uint64_t remaining_ticks =
                deadline_tick > current_tick ? deadline_tick - current_tick : 0U;
            const SelectTimeValue remaining{
                static_cast<int64_t>(remaining_ticks / kSchedulerTicksPerSecond),
                static_cast<int64_t>((remaining_ticks % kSchedulerTicksPerSecond) *
                                     (1000000ULL / kSchedulerTicksPerSecond)),
            };
            return copy_to_user(address, &remaining, sizeof(remaining)) == 0;
        }

        [[nodiscard]] bool timeout_to_deadline(const SelectTimeValue &timeout,
                                               uint64_t &deadline_tick) noexcept {
            if (timeout.seconds < 0 || timeout.microseconds < 0 ||
                timeout.microseconds >= 1000000) {
                return false;
            }
            const uint64_t seconds = static_cast<uint64_t>(timeout.seconds);
            if (seconds > std::numeric_limits<uint64_t>::max() / kSchedulerTicksPerSecond) {
                return false;
            }
            uint64_t duration_ticks = seconds * kSchedulerTicksPerSecond;
            duration_ticks += (static_cast<uint64_t>(timeout.microseconds) +
                               (1000000ULL / kSchedulerTicksPerSecond) - 1U) /
                              (1000000ULL / kSchedulerTicksPerSecond);
            const uint64_t current_tick = g_unified_scheduler.tick_count();
            if (duration_ticks > std::numeric_limits<uint64_t>::max() - current_tick) {
                return false;
            }
            deadline_tick = current_tick + duration_ticks;
            return true;
        }

    } // namespace

    int64_t process_select(int descriptor_count, uintptr_t read_address, uintptr_t write_address,
                           uintptr_t exception_address, uintptr_t timeout_address) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        if (descriptor_count < 0 || descriptor_count > static_cast<int>(MAX_FDS_PER_PROCESS)) {
            clear_select_state(*process);
            return -EINVAL;
        }

        DescriptorSet requested_read{};
        DescriptorSet requested_write{};
        DescriptorSet requested_exception{};
        if (!copy_descriptor_set(read_address, requested_read) ||
            !copy_descriptor_set(write_address, requested_write) ||
            !copy_descriptor_set(exception_address, requested_exception)) {
            clear_select_state(*process);
            return -EFAULT;
        }

        for (int descriptor = 0; descriptor < descriptor_count; ++descriptor) {
            if (any_set_contains(requested_read, requested_write, requested_exception, read_address,
                                 write_address, exception_address, descriptor) &&
                !process->fd_table.is_valid_fd(descriptor)) {
                clear_select_state(*process);
                return -EBADF;
            }
        }

        uint64_t deadline_tick = 0U;
        if (process->select_active) {
            deadline_tick = process->select_deadline_tick;
            timeout_address = process->select_timeout_address;
        } else if (timeout_address != 0U) {
            SelectTimeValue timeout{};
            if (copy_from_user(&timeout, timeout_address, sizeof(timeout)) != 0) {
                return -EFAULT;
            }
            if (!timeout_to_deadline(timeout, deadline_tick)) {
                return -EINVAL;
            }
        }

        DescriptorSet ready_read{};
        DescriptorSet ready_write{};
        DescriptorSet ready_exception{};
        int ready_count = 0;
        for (int descriptor = 0; descriptor < descriptor_count; ++descriptor) {
            if (read_address != 0U && descriptor_is_set(requested_read, descriptor) &&
                bootfs_descriptor_read_ready(descriptor)) {
                set_descriptor(ready_read, descriptor);
                ++ready_count;
            }
            if (write_address != 0U && descriptor_is_set(requested_write, descriptor) &&
                bootfs_descriptor_write_ready(descriptor)) {
                set_descriptor(ready_write, descriptor);
                ++ready_count;
            }
        }

        const uint64_t current_tick = g_unified_scheduler.tick_count();
        const bool deadline_reached = timeout_address != 0U && current_tick >= deadline_tick;
        if (ready_count != 0 || deadline_reached) {
            if (!write_descriptor_set(read_address, ready_read) ||
                !write_descriptor_set(write_address, ready_write) ||
                !write_descriptor_set(exception_address, ready_exception) ||
                !write_remaining_timeout(timeout_address, deadline_tick)) {
                clear_select_state(*process);
                return -EFAULT;
            }
            clear_select_state(*process);
            return ready_count;
        }

        if (timeout_address != 0U && !write_remaining_timeout(timeout_address, deadline_tick)) {
            clear_select_state(*process);
            return -EFAULT;
        }
        process_block_for_select(deadline_tick, timeout_address);
    }

} // namespace xinim::kernel::x86_64
