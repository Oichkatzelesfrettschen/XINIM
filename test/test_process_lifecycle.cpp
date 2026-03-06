/**
 * @file test_process_lifecycle.cpp
 * @brief Host-side tests for process exit and wait (v1.2.0).
 */

#include "unified_scheduler.hpp"
#include "process_lifecycle.hpp"
#include <cassert>
#include <cstring>

using namespace xinim::kernel;

static ProcessControlBlock make_pcb(xinim::pid_t pid, uint32_t priority,
                                     xinim::pid_t parent = 0) {
    ProcessControlBlock pcb{};
    memset(&pcb, 0, sizeof(pcb));
    pcb.pid = pid;
    pcb.priority = priority;
    pcb.state = ProcessState::READY;
    pcb.parent_pid = parent;
    pcb.blocked_on = BlockReason::NONE;
    pcb.ipc_wait_source = -1;
    pcb.has_exited = false;
    pcb.has_been_waited = false;
    pcb.exit_status = 0;
    pcb.stack_base = nullptr;
    pcb.stack_size = 0;
    pcb.kernel_stack_base = nullptr;
    pcb.kernel_stack_size = 0;
    return pcb;
}

static void test_process_exit_marks_zombie() {
    // Reset the global scheduler
    g_unified_scheduler = UnifiedScheduler();

    ProcessControlBlock pcb = make_pcb(5, PRIO_USER_NORM);
    g_unified_scheduler.add_process(&pcb);
    g_unified_scheduler.pick_next(); // Make it current

    process_exit(5, 42);

    assert(pcb.state == ProcessState::ZOMBIE);
    assert(pcb.exit_status == 42);
    assert(pcb.has_exited == true);
}

static void test_process_wait_reaps_zombie() {
    g_unified_scheduler = UnifiedScheduler();

    ProcessControlBlock parent = make_pcb(1, PRIO_USER_NORM);
    ProcessControlBlock child  = make_pcb(2, PRIO_USER_NORM, 1); // parent_pid = 1

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

    ProcessControlBlock parent = make_pcb(1, PRIO_USER_NORM);
    ProcessControlBlock c1     = make_pcb(2, PRIO_USER_NORM, 1);
    ProcessControlBlock c2     = make_pcb(3, PRIO_USER_NORM, 1);

    g_unified_scheduler.add_process(&parent);
    g_unified_scheduler.add_process(&c1);
    g_unified_scheduler.add_process(&c2);

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

    ProcessControlBlock lonely = make_pcb(1, PRIO_USER_NORM);
    g_unified_scheduler.add_process(&lonely);

    int status = 0;
    xinim::pid_t result = process_wait(1, -1, &status);
    assert(result == -1); // No children
}

static void test_exit_notifies_waiting_parent() {
    g_unified_scheduler = UnifiedScheduler();

    ProcessControlBlock parent = make_pcb(1, PRIO_USER_NORM);
    ProcessControlBlock child  = make_pcb(2, PRIO_USER_NORM, 1);

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
    CpuContext ctx{};
    ctx.initialize(0x1000, 0x2000, 0);

    // MXCSR at offset 24 should be 0x1F80
    uint32_t mxcsr = 0;
    mxcsr |= static_cast<uint32_t>(ctx.fxsave_area[24]);
    mxcsr |= static_cast<uint32_t>(ctx.fxsave_area[25]) << 8;
    assert(mxcsr == 0x1F80);

    // Rest of fxsave should be zero
    for (int i = 0; i < 512; i++) {
        if (i == 24 || i == 25) continue;
        assert(ctx.fxsave_area[i] == 0);
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
