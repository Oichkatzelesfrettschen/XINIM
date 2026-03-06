/**
 * @file test_unified_scheduler.cpp
 * @brief Host-side unit tests for the O(1) bitmap unified scheduler.
 *
 * Tests: O(1) pick_next correctness, priority ordering, quantum expiry,
 * deadlock detection, block/unblock round-trip, bitmap consistency.
 */

#include "unified_scheduler.hpp"
#include <cassert>
#include <cstring>

using namespace xinim::kernel;

// Helper to create a minimal PCB for testing
static ProcessControlBlock make_pcb(xinim::pid_t pid, uint32_t priority) {
    ProcessControlBlock pcb{};
    memset(&pcb, 0, sizeof(pcb));
    pcb.pid = pid;
    pcb.priority = priority;
    pcb.state = ProcessState::READY;
    pcb.next = nullptr;
    pcb.prev = nullptr;
    pcb.blocked_on = BlockReason::NONE;
    pcb.ipc_wait_source = -1;
    return pcb;
}

static void test_empty_scheduler() {
    UnifiedScheduler s;
    assert(s.pick_next() == nullptr);
    assert(s.current() == nullptr);
    assert(s.current_pid() == -1);
}

static void test_single_process() {
    UnifiedScheduler s;
    ProcessControlBlock pcb = make_pcb(1, PRIO_USER_NORM);

    s.add_process(&pcb);
    auto* next = s.pick_next();
    assert(next == &pcb);
    assert(s.current_pid() == 1);

    // Queue is now empty
    assert(s.pick_next() == nullptr);
}

static void test_priority_ordering() {
    UnifiedScheduler s;
    ProcessControlBlock low  = make_pcb(1, PRIO_USER_LOW);   // priority 32
    ProcessControlBlock high = make_pcb(2, PRIO_SERVER_LO);  // priority 4
    ProcessControlBlock mid  = make_pcb(3, PRIO_USER_NORM);  // priority 16

    s.add_process(&low);
    s.add_process(&high);
    s.add_process(&mid);

    // Highest priority (lowest number) should be picked first
    auto* first = s.pick_next();
    assert(first->pid == 2); // PRIO_SERVER_LO = 4

    auto* second = s.pick_next();
    assert(second->pid == 3); // PRIO_USER_NORM = 16

    auto* third = s.pick_next();
    assert(third->pid == 1); // PRIO_USER_LOW = 32

    assert(s.pick_next() == nullptr);
}

static void test_same_priority_fifo() {
    UnifiedScheduler s;
    ProcessControlBlock a = make_pcb(1, PRIO_USER_NORM);
    ProcessControlBlock b = make_pcb(2, PRIO_USER_NORM);
    ProcessControlBlock c = make_pcb(3, PRIO_USER_NORM);

    s.add_process(&a);
    s.add_process(&b);
    s.add_process(&c);

    // Same priority: FIFO ordering
    assert(s.pick_next()->pid == 1);
    assert(s.pick_next()->pid == 2);
    assert(s.pick_next()->pid == 3);
    assert(s.pick_next() == nullptr);
}

static void test_block_unblock() {
    UnifiedScheduler s;
    ProcessControlBlock a = make_pcb(1, PRIO_USER_NORM);
    ProcessControlBlock b = make_pcb(2, PRIO_USER_NORM);

    s.add_process(&a);
    s.add_process(&b);

    // Block process 2
    bool ok = s.block(&b, BlockReason::IPC_RECV, -1);
    assert(ok);
    assert(b.state == ProcessState::BLOCKED);

    // Only process 1 should be runnable
    assert(s.pick_next()->pid == 1);
    assert(s.pick_next() == nullptr);

    // Unblock process 2
    s.unblock(2);
    assert(b.state == ProcessState::READY);
    assert(s.pick_next()->pid == 2);
}

static void test_deadlock_detection() {
    UnifiedScheduler s;
    ProcessControlBlock a = make_pcb(1, PRIO_USER_NORM);
    ProcessControlBlock b = make_pcb(2, PRIO_USER_NORM);

    s.add_process(&a);
    s.add_process(&b);

    // Pick a to make it current
    s.pick_next();

    // Block a waiting for b
    bool ok1 = s.block(&a, BlockReason::IPC_RECV, 2);
    assert(ok1);

    // Block b waiting for a -- should detect deadlock cycle
    bool ok2 = s.block(&b, BlockReason::IPC_RECV, 1);
    assert(!ok2); // Deadlock! Must return false
    assert(b.state != ProcessState::BLOCKED); // b should NOT be blocked
}

static void test_yield() {
    UnifiedScheduler s;
    ProcessControlBlock a = make_pcb(1, PRIO_USER_NORM);
    ProcessControlBlock b = make_pcb(2, PRIO_USER_NORM);

    s.add_process(&a);
    s.add_process(&b);

    s.pick_next(); // a is current
    assert(s.current_pid() == 1);

    s.yield(); // a goes back to queue, b becomes current
    assert(s.current_pid() == 2);
}

static void test_yield_to() {
    UnifiedScheduler s;
    ProcessControlBlock a = make_pcb(1, PRIO_USER_NORM);
    ProcessControlBlock b = make_pcb(2, PRIO_USER_LOW);

    s.add_process(&a);
    s.add_process(&b);

    s.pick_next(); // a is current (higher priority)
    assert(s.current_pid() == 1);

    s.yield_to(2); // Directly switch to b
    assert(s.current_pid() == 2);

    // a should be back in the queue
    auto* next = s.pick_next();
    assert(next->pid == 1);
}

static void test_quantum_values() {
    // Verify quantum table
    assert(quantum_for_priority(0) == 0);  // System: unlimited
    assert(quantum_for_priority(3) == 0);
    assert(quantum_for_priority(4) == 20); // Server
    assert(quantum_for_priority(7) == 20);
    assert(quantum_for_priority(8) == 10); // User high
    assert(quantum_for_priority(15) == 10);
    assert(quantum_for_priority(16) == 8); // User normal
    assert(quantum_for_priority(31) == 8);
    assert(quantum_for_priority(32) == 4); // Background
    assert(quantum_for_priority(47) == 4);
    assert(quantum_for_priority(48) == 1); // Idle
    assert(quantum_for_priority(63) == 1);
}

static void test_timer_tick_quantum_expiry() {
    UnifiedScheduler s;
    ProcessControlBlock a = make_pcb(1, PRIO_USER_NORM); // quantum = 8
    ProcessControlBlock b = make_pcb(2, PRIO_USER_NORM);

    s.add_process(&a);
    s.add_process(&b);

    s.pick_next(); // a is current
    assert(s.current_pid() == 1);

    // Tick 7 times -- should stay on a
    for (int i = 0; i < 7; i++) {
        s.timer_tick();
        assert(s.current_pid() == 1);
    }

    // 8th tick should cause yield (quantum expired)
    s.timer_tick();
    assert(s.current_pid() == 2); // b is now current
}

static void test_system_task_no_preemption() {
    UnifiedScheduler s;
    ProcessControlBlock sys = make_pcb(1, PRIO_SYSTEM_LO); // quantum = 0 (unlimited)
    ProcessControlBlock usr = make_pcb(2, PRIO_USER_NORM);

    s.add_process(&sys);
    s.add_process(&usr);

    s.pick_next(); // sys is current
    assert(s.current_pid() == 1);

    // Tick many times -- system task should not be preempted
    for (int i = 0; i < 100; i++) {
        s.timer_tick();
    }
    assert(s.current_pid() == 1); // Still running
}

static void test_bitmap_consistency() {
    UnifiedScheduler s;
    ProcessControlBlock a = make_pcb(1, 5);  // Server priority
    ProcessControlBlock b = make_pcb(2, 20); // User normal

    s.add_process(&a);
    s.add_process(&b);

    // Both queues should have bits set
    auto* p1 = s.pick_next();
    assert(p1->pid == 1); // Priority 5 first

    auto* p2 = s.pick_next();
    assert(p2->pid == 2); // Priority 20 next

    // Both dequeued -- pick_next should return nullptr
    assert(s.pick_next() == nullptr);
}

static void test_find_by_pid() {
    UnifiedScheduler s;
    ProcessControlBlock a = make_pcb(5, PRIO_USER_NORM);

    s.add_process(&a);
    assert(s.find_by_pid(5) == &a);
    assert(s.find_by_pid(99) == nullptr);
    assert(s.find_by_pid(-1) == nullptr);
}

int main() {
    test_empty_scheduler();
    test_single_process();
    test_priority_ordering();
    test_same_priority_fifo();
    test_block_unblock();
    test_deadlock_detection();
    test_yield();
    test_yield_to();
    test_quantum_values();
    test_timer_tick_quantum_expiry();
    test_system_task_no_preemption();
    test_bitmap_consistency();
    test_find_by_pid();
    return 0;
}
