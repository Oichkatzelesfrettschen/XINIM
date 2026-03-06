/**
 * @file test_ipc_exhaustion.cpp
 * @brief Tests for IPC channel table exhaustion (v1.2.0).
 *
 * Verifies that connect() returns nullptr (not silent corruption)
 * when the channel table is full.
 */

#include "../kernel/lattice_ipc.hpp"
#include <cassert>

static void test_channel_exhaustion_returns_null() {
    lattice::Graph graph;

    // Fill the channel table
    for (int i = 0; i < lattice::Graph::MAX_CHANNELS; i++) {
        auto* ch = graph.connect(i % 64, (i + 1) % 64, 0);
        // Early channels may reuse existing ones, but eventually
        // we should fill the table. Just keep going.
        (void)ch;
    }

    // Now try to connect a completely new pair that doesn't exist yet
    // Use PIDs that couldn't have been used above (trick: same src/dst)
    // Actually, let's just check that after MAX_CHANNELS unique connections,
    // the next unique one fails.
    lattice::Graph graph2;
    // Create MAX_CHANNELS unique channels (each with unique src)
    for (int i = 0; i < lattice::Graph::MAX_CHANNELS; i++) {
        auto* ch = graph2.connect(static_cast<xinim::pid_t>(i % 64),
                                   static_cast<xinim::pid_t>((i / 64) % 64), i);
        // Most should succeed (different node_id makes them unique)
        assert(ch != nullptr);
    }

    // Table is now full -- next unique connect must return nullptr
    auto* overflow = graph2.connect(63, 63, 999);
    assert(overflow == nullptr);
}

static void test_existing_channel_still_found() {
    lattice::Graph graph;

    auto* ch1 = graph.connect(1, 2, 0);
    assert(ch1 != nullptr);

    // Connecting the same pair should return the existing channel
    auto* ch2 = graph.connect(1, 2, 0);
    assert(ch2 == ch1);
}

int main() {
    test_channel_exhaustion_returns_null();
    test_existing_channel_still_found();
    return 0;
}
