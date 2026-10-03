#include "../kernel/schedule.hpp"
#include "../kernel/wormhole.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

using namespace fastpath;

/**
 * @brief Benchmark helper executing @p iters fastpath runs.
 */
static double bench(State &state, std::size_t iters) {
    using clock = std::chrono::steady_clock;
    auto start = clock::now();
    for (std::size_t i = 0; i < iters; ++i) {
        state.sender.status = ThreadStatus::Running;
        state.receiver.status = ThreadStatus::RecvBlocked;
        state.endpoint.state = EndpointState::Recv;
        state.endpoint.queue = {state.receiver.tid};
        execute_fastpath(state);
        sched::scheduler.enqueue(state.sender.tid);
        sched::scheduler.enqueue(state.receiver.tid);
        sched::scheduler.preempt();
    }
    auto end = clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count();
}

int main() {
    using sched::scheduler;
    State s{};
    scheduler.enqueue(1);
    scheduler.enqueue(2);
    scheduler.preempt();

    s.sender.tid = 1;
    s.sender.priority = 5;
    s.sender.domain = 0;
    s.sender.core = 0;
    s.receiver.tid = 2;
    s.receiver.priority = 5;
    s.receiver.domain = 0;
    s.receiver.core = 0;
    s.endpoint.eid = 1;
    s.cap.cptr = 1;
    s.cap.type = CapType::Endpoint;
    s.cap.rights.write = true;
    s.cap.object = 1;
    s.msg_len = 1;

    alignas(64) uint64_t l1[8]{};
    alignas(64) uint64_t l2[8]{};
    alignas(64) uint64_t l3[8]{};
    alignas(64) uint64_t main_buf[8]{};
    s.l1_buffer = MessageRegion(reinterpret_cast<std::uintptr_t>(l1), sizeof(l1));
    s.l2_buffer = MessageRegion(reinterpret_cast<std::uintptr_t>(l2), sizeof(l2));
    s.l3_buffer = MessageRegion(reinterpret_cast<std::uintptr_t>(l3), sizeof(l3));
    set_message_region(s,
                       MessageRegion(reinterpret_cast<std::uintptr_t>(main_buf), sizeof(main_buf)));
    s.current_tid = scheduler.current();

    const std::size_t loops = 1000;
    double t_l1 = bench(s, loops);

    s.l1_buffer = {0, 0};
    double t_l2 = bench(s, loops);

    s.l2_buffer = {0, 0};
    double t_l3 = bench(s, loops);

    s.l3_buffer = {0, 0};
    double t_main = bench(s, loops);

    assert(t_l1 > 0);
    assert(t_l2 > 0);
    assert(t_l3 > 0);
    assert(t_main > 0);
    std::cout << "L1: " << t_l1 << " us\n"
              << "L2: " << t_l2 << " us\n"
              << "L3: " << t_l3 << " us\n"
              << "Main: " << t_main << " us\n";
    return 0;
}
