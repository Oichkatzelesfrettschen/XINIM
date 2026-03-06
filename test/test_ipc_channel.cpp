/**
 * @file test_ipc_channel.cpp
 * @brief Unit tests for lattice IPC Channel (push/pop queue semantics).
 *
 * WHY: The lattice IPC Channel is the core message transport primitive.
 *      These tests verify queue FIFO ordering, overflow handling, and
 *      empty-pop behavior without requiring full kernel context.
 *
 * NOTE: This test links only lattice_ipc.cpp which provides Channel and Graph.
 *       The lattice_send/lattice_recv functions require a running graph, which
 *       is tested at the channel level here.
 */

#include "../kernel/lattice_ipc.hpp"
#include <cassert>

static void test_channel_push_pop_fifo() {
    lattice::Channel ch;
    ch.src = 1;
    ch.dst = 2;

    message m1{};
    m1.m_type = 100;
    message m2{};
    m2.m_type = 200;

    assert(ch.push(m1));
    assert(ch.push(m2));

    message out{};
    assert(ch.pop(out));
    assert(out.m_type == 100); // FIFO: first in, first out

    assert(ch.pop(out));
    assert(out.m_type == 200);

    // Queue should now be empty
    assert(!ch.pop(out));
}

static void test_channel_overflow() {
    lattice::Channel ch;
    message m{};

    // Fill the queue (v1.2.0: QUEUE_SIZE is now 32)
    for (std::size_t i = 0; i < lattice::Channel::QUEUE_SIZE; ++i) {
        m.m_type = static_cast<int>(i);
        assert(ch.push(m));
    }

    // Next push must fail (queue full)
    m.m_type = 999;
    assert(!ch.push(m));

    // Count must remain at QUEUE_SIZE
    assert(ch.count == lattice::Channel::QUEUE_SIZE);
}

static void test_channel_empty_pop() {
    lattice::Channel ch;
    message out{};
    // Pop on empty queue must return false
    assert(!ch.pop(out));
}

static void test_channel_wrap_around() {
    lattice::Channel ch;

    // Push and pop alternately to exercise ring buffer wrap-around
    for (int round = 0; round < 3; ++round) {
        for (int i = 0; i < static_cast<int>(lattice::Channel::QUEUE_SIZE); ++i) {
            message m{};
            m.m_type = round * 100 + i;
            assert(ch.push(m));
        }
        for (int i = 0; i < static_cast<int>(lattice::Channel::QUEUE_SIZE); ++i) {
            message out{};
            assert(ch.pop(out));
            assert(out.m_type == round * 100 + i);
        }
    }
}

int main() {
    test_channel_push_pop_fifo();
    test_channel_overflow();
    test_channel_empty_pop();
    test_channel_wrap_around();
    return 0;
}
