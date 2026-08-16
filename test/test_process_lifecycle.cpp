/**
 * @file test_process_lifecycle.cpp
 * @brief Host-side tests for process exit and wait.
 */

#include "arch/x86_64/user_address_space.hpp"
#include "process_lifecycle.hpp"
#include "unified_scheduler.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>

using namespace xinim::kernel;

namespace {

    std::uint64_t destroyed_address_space_root = 0U;
    std::size_t address_space_destruction_count = 0U;

} // namespace

namespace xinim::kernel {

    int send_signal(ProcessControlBlock * /*process*/, int /*signal_number*/) noexcept {
        return 0;
    }

    namespace x86_64 {

        void destroy_user_address_space(UserAddressSpace &address_space) noexcept {
            ::destroyed_address_space_root = address_space.root_physical;
            ++::address_space_destruction_count;
            address_space.root_physical = 0U;
        }

    } // namespace x86_64
} // namespace xinim::kernel

static ProcessControlBlock make_process(xinim::pid_t process_id, std::uint32_t priority,
                                        xinim::pid_t parent_id = 0) {
    ProcessControlBlock process{};
    process.pid = process_id;
    process.priority = priority;
    process.state = ProcessState::READY;
    process.parent_pid = parent_id;
    process.blocked_on = BlockReason::NONE;
    process.ipc_wait_source = -1;
    return process;
}

static void test_process_exit_marks_zombie() {
    // Reset the global scheduler
    g_unified_scheduler = UnifiedScheduler();

    ProcessControlBlock process = make_process(5, PRIO_USER_NORM);
    process.address_space_root = 0x4000U;
    process.context.cr3 = process.address_space_root;
    destroyed_address_space_root = 0U;
    address_space_destruction_count = 0U;
    g_unified_scheduler.add_process(&process);
    g_unified_scheduler.pick_next(); // Make it current

    process_exit(5, 42);

    assert(process.state == ProcessState::ZOMBIE);
    assert(process.exit_status == 42);
    assert(process.has_exited == true);
    assert(address_space_destruction_count == 1U);
    assert(destroyed_address_space_root == 0x4000U);
    assert(process.address_space_root == 0U);
    assert(process.context.cr3 == 0U);
}

static void test_process_wait_reaps_zombie() {
    g_unified_scheduler = UnifiedScheduler();

    ProcessControlBlock parent = make_process(1, PRIO_USER_NORM);
    ProcessControlBlock child = make_process(2, PRIO_USER_NORM, 1); // parent_pid = 1

    g_unified_scheduler.add_process(&parent);
    g_unified_scheduler.add_process(&child);

    // Exit child
    process_exit(2, 99);
    assert(child.state == ProcessState::ZOMBIE);

    // Parent waits
    int status = 0;
    xinim::pid_t reaped = process_wait(1, 2, &status);
    assert(reaped == 2);
    assert(status == 99);
    assert(child.state == ProcessState::DEAD);
    assert(child.has_been_waited == true);
}

static void test_wait_any_child() {
    g_unified_scheduler = UnifiedScheduler();

    ProcessControlBlock parent = make_process(1, PRIO_USER_NORM);
    ProcessControlBlock first_child = make_process(2, PRIO_USER_NORM, 1);
    ProcessControlBlock second_child = make_process(3, PRIO_USER_NORM, 1);

    g_unified_scheduler.add_process(&parent);
    g_unified_scheduler.add_process(&first_child);
    g_unified_scheduler.add_process(&second_child);

    // Exit child 3
    process_exit(3, 7);

    // Wait for any child (-1)
    int status = 0;
    xinim::pid_t reaped = process_wait(1, -1, &status);
    assert(reaped == 3);
    assert(status == 7);
}

static void test_wait_no_children() {
    g_unified_scheduler = UnifiedScheduler();

    ProcessControlBlock lonely = make_process(1, PRIO_USER_NORM);
    g_unified_scheduler.add_process(&lonely);

    int status = 0;
    xinim::pid_t result = process_wait(1, -1, &status);
    assert(result == -1); // No children
}

static void test_exit_notifies_waiting_parent() {
    g_unified_scheduler = UnifiedScheduler();

    ProcessControlBlock parent = make_process(1, PRIO_USER_NORM);
    ProcessControlBlock child = make_process(2, PRIO_USER_NORM, 1);

    g_unified_scheduler.add_process(&parent);
    g_unified_scheduler.add_process(&child);
    g_unified_scheduler.pick_next(); // parent runs

    // Parent waits but child is still alive -- parent blocks
    int status = 0;
    xinim::pid_t result = process_wait(1, 2, &status);
    assert(result == 0); // Blocked, will retry
    assert(parent.state == ProcessState::BLOCKED);

    // Now child exits -- should unblock parent
    process_exit(2, 55);
    assert(parent.state == ProcessState::READY);
}

static void test_fxsave_area_initialization() {
    CpuContext context{};
    context.initialize(0x1000U, 0x2000U, 0U);

    const auto read_little_endian_u16 = [&context](std::size_t byte_offset) {
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(context.fxsave_area[byte_offset]) |
            (static_cast<std::uint16_t>(context.fxsave_area[byte_offset + 1U]) << 8U));
    };
    constexpr auto control_word_offset = static_cast<std::size_t>(XINIM_X86_64_FXSAVE_FCW_OFFSET);
    constexpr auto mxcsr_offset = static_cast<std::size_t>(XINIM_X86_64_FXSAVE_MXCSR_OFFSET);

    assert(read_little_endian_u16(control_word_offset) == XINIM_X86_64_FXSAVE_RESET_FCW);
    assert(read_little_endian_u16(mxcsr_offset) == XINIM_X86_64_FXSAVE_RESET_MXCSR);

    for (std::size_t byte_index = 0; byte_index < std::size(context.fxsave_area); ++byte_index) {
        const bool is_control_word =
            byte_index == control_word_offset || byte_index == control_word_offset + 1U;
        const bool is_mxcsr = byte_index == mxcsr_offset || byte_index == mxcsr_offset + 1U;
        if (is_control_word || is_mxcsr)
            continue;
        assert(context.fxsave_area[byte_index] == 0U);
    }
}

int main() {
    test_process_exit_marks_zombie();
    test_process_wait_reaps_zombie();
    test_wait_any_child();
    test_wait_no_children();
    test_exit_notifies_waiting_parent();
    test_fxsave_area_initialization();
    return 0;
}
