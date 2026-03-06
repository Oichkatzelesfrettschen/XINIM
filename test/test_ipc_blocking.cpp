/**
 * @file test_ipc_blocking.cpp
 * @brief Tests for IPC blocking send/recv semantics (v1.2.0).
 *
 * Tests channel back-pressure, blocked sender tracking, source tagging,
 * and the increased queue depth.
 */

#include "../kernel/lattice_ipc.hpp"
#include <cassert>
#include <cstring>

static void test_increased_queue_depth() {
    lattice::Channel ch;
    ch.src = 1;
    ch.dst = 2;

    // Should be able to push 32 messages (up from 8)
    for (std::size_t i = 0; i < lattice::Channel::QUEUE_SIZE; i++) {
        message m{};
        m.m_type = static_cast<int>(i);
        assert(ch.push(m));
    }
    assert(ch.count == 32);

    // 33rd push should fail
    message overflow{};
    overflow.m_type = 999;
    assert(!ch.push(overflow));

    // Pop all and verify FIFO
    for (std::size_t i = 0; i < lattice::Channel::QUEUE_SIZE; i++) {
        message out{};
        assert(ch.pop(out));
        assert(out.m_type == static_cast<int>(i));
    }
    assert(!ch.pop(overflow));
}

static void test_blocked_sender_tracking() {
    lattice::Channel ch;

    // Add blocked senders
    ch.add_blocked_sender(10);
    ch.add_blocked_sender(20);
    ch.add_blocked_sender(30);

    assert(ch.blocked_sender_count == 3);

    // Pop in FIFO order
    assert(ch.pop_blocked_sender() == 10);
    assert(ch.pop_blocked_sender() == 20);
    assert(ch.pop_blocked_sender() == 30);
    assert(ch.pop_blocked_sender() == -1); // Empty

    // Verify max capacity
    for (int i = 0; i < lattice::Channel::MAX_BLOCKED_SENDERS; i++) {
        ch.add_blocked_sender(static_cast<xinim::pid_t>(i));
    }
    assert(ch.blocked_sender_count == lattice::Channel::MAX_BLOCKED_SENDERS);

    // Adding beyond max should be silently ignored
    ch.add_blocked_sender(99);
    assert(ch.blocked_sender_count == lattice::Channel::MAX_BLOCKED_SENDERS);
}

static void test_source_tagging_on_channel() {
    // When a message is pushed, verify it carries the m_source field
    lattice::Channel ch;
    ch.src = 5;
    ch.dst = 10;

    message m{};
    m.m_type = 42;
    m.m_source = 5; // Sender tags before push

    assert(ch.push(m));

    message out{};
    assert(ch.pop(out));
    assert(out.m_source == 5);
    assert(out.m_type == 42);
}

static void test_incoming_channel_bitset() {
    lattice::Graph graph;

    // Mark that process 3 has incoming from process 7
    graph.mark_incoming(3, 7);
    uint64_t bits = graph.get_incoming(3);
    assert(bits == (1ULL << 7));

    // Mark another
    graph.mark_incoming(3, 2);
    bits = graph.get_incoming(3);
    assert(bits == ((1ULL << 7) | (1ULL << 2)));

    // Different process
    assert(graph.get_incoming(5) == 0);
}

static void test_wrap_around_larger_queue() {
    lattice::Channel ch;

    // Push and pop repeatedly to exercise ring buffer with larger size
    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < static_cast<int>(lattice::Channel::QUEUE_SIZE); i++) {
            message m{};
            m.m_type = round * 1000 + i;
            assert(ch.push(m));
        }
        for (int i = 0; i < static_cast<int>(lattice::Channel::QUEUE_SIZE); i++) {
            message out{};
            assert(ch.pop(out));
            assert(out.m_type == round * 1000 + i);
        }
    }
}

int main() {
    test_increased_queue_depth();
    test_blocked_sender_tracking();
    test_source_tagging_on_channel();
    test_incoming_channel_bitset();
    test_wrap_around_larger_queue();
    return 0;
}
