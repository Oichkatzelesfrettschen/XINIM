/* This file contains the clock task.
 */

#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include "panic.hpp"
#include <cstddef>
#include <cstdint>

// Forward declarations
static void do_setalarm(message *m_ptr) noexcept;
static void do_get_time() noexcept;
static void do_set_time(message *m_ptr) noexcept;
static void do_clocktick() noexcept;
static void init_clock() noexcept;

static message mc;

/// System tick counter (incremented on each clock interrupt).
static uint64_t system_ticks = 0;

/// Boot time in seconds (set by SET_TIME).
static uint64_t boot_time = 0;

/// Per-process quantum tracking.
static constexpr int DEFAULT_QUANTUM = 10; // ticks per time slice

extern "C" void clock_task() noexcept {
    int opcode;
    init_clock();
    while (TRUE) {
        ipc_receive(ANY, &mc);
        opcode = mc.m_type;
        switch (opcode) {
        case SET_ALARM:  do_setalarm(&mc); break;
        case GET_TIME:   do_get_time(); break;
        case SET_TIME:   do_set_time(&mc); break;
        case CLOCK_TICK: do_clocktick(); break;
        default:         kpanic("clock task got bad message");
        }
        mc.m_type = OK;
        if (opcode != CLOCK_TICK)
            ipc_send(mc.m_source, &mc);
    }
}

static void do_setalarm([[maybe_unused]] message *m_ptr) noexcept {
    // TODO Phase-6: implement alarm list with tick-based expiration
}

static void do_get_time() noexcept {
    // Return time in seconds via m1_i1 (reusing MINIX convention)
    mc.m1_i1() = static_cast<int>(boot_time + system_ticks / HZ);
}

static void do_set_time(message *m_ptr) noexcept {
    boot_time = static_cast<uint64_t>(m_ptr->m1_i1());
    system_ticks = 0;
}

static void do_clocktick() noexcept {
    system_ticks++;

    // Decrement current process quantum; preempt if expired
    if (proc_ptr != nullptr && proc_ptr != proc_addr(HARDWARE)) {
        proc_ptr->user_time++;
        if (proc_ptr->user_time % DEFAULT_QUANTUM == 0) {
            kernel_sched();
        }
    }
}

static void init_clock() noexcept {
    system_ticks = 0;
    boot_time = 0;
}
