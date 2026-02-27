/**
 * @file test_wait_graph_stress.cpp
 * @brief Stress tests for lattice::WaitForGraph.
 *
 * Tests edge saturation, long chains, repeated add/remove, and
 * multiple independent cycles.
 */

#include "wait_graph.hpp"
#include <cassert>

static void test_long_chain() {
    lattice::WaitForGraph g;

    // Build chain: 0 -> 1 -> 2 -> ... -> 30
    for (int i = 0; i < 30; ++i) {
        g.add_edge(i, i + 1);
    }

    // Path exists from start to end
    assert(g.has_path(0, 30));
    assert(g.has_path(0, 15));
    assert(!g.has_path(30, 0));

    // No cycles yet
    for (int i = 0; i <= 30; ++i) {
        assert(!g.is_in_cycle(i));
    }

    // Close the chain to create a large cycle
    g.add_edge(30, 0);
    for (int i = 0; i <= 30; ++i) {
        assert(g.is_in_cycle(i));
    }

    // Break the cycle
    g.remove_edge(15, 16);
    assert(!g.is_in_cycle(0));
    assert(!g.is_in_cycle(30));
}

static void test_multiple_independent_cycles() {
    lattice::WaitForGraph g;

    // Cycle A: 0 -> 1 -> 2 -> 0
    g.add_edge(0, 1);
    g.add_edge(1, 2);
    g.add_edge(2, 0);

    // Cycle B: 10 -> 11 -> 12 -> 10
    g.add_edge(10, 11);
    g.add_edge(11, 12);
    g.add_edge(12, 10);

    assert(g.is_in_cycle(0));
    assert(g.is_in_cycle(1));
    assert(g.is_in_cycle(2));
    assert(g.is_in_cycle(10));
    assert(g.is_in_cycle(11));
    assert(g.is_in_cycle(12));

    // No path between independent cycles
    assert(!g.has_path(0, 10));
    assert(!g.has_path(10, 0));

    // Breaking one cycle doesn't affect the other
    g.remove_edge(2, 0);
    assert(!g.is_in_cycle(0));
    assert(g.is_in_cycle(10));
}

static void test_add_remove_churn() {
    lattice::WaitForGraph g;

    // Rapid add/remove cycles
    for (int iter = 0; iter < 50; ++iter) {
        int a = iter % 32;
        int b = (iter + 7) % 32;
        if (a == b) b = (b + 1) % 32;

        g.add_edge(a, b);
        g.add_edge(b, a);
        assert(g.is_in_cycle(a));
        assert(g.is_in_cycle(b));

        g.remove_edge(a, b);
        g.remove_edge(b, a);
        assert(!g.is_in_cycle(a));
        assert(!g.is_in_cycle(b));
    }
}

static void test_self_loops() {
    lattice::WaitForGraph g;

    // Self-loop: node waits on itself
    g.add_edge(5, 5);
    assert(g.is_in_cycle(5));

    g.remove_edge(5, 5);
    assert(!g.is_in_cycle(5));

    // Multiple self-loops
    for (int i = 0; i < 10; ++i) {
        g.add_edge(i, i);
        assert(g.is_in_cycle(i));
    }
}

static void test_out_of_range_ids() {
    lattice::WaitForGraph g;

    // IDs outside the valid range should be handled gracefully
    g.add_edge(-1, 5);
    g.add_edge(5, -1);
    g.add_edge(100, 200);

    // Valid nodes should be unaffected
    assert(!g.is_in_cycle(5));
    assert(!g.has_path(-1, 5));
    assert(!g.has_path(100, 200));
}

static void test_diamond_graph() {
    lattice::WaitForGraph g;

    //     1
    //    / \
    //   2   3
    //    \ /
    //     4
    g.add_edge(1, 2);
    g.add_edge(1, 3);
    g.add_edge(2, 4);
    g.add_edge(3, 4);

    assert(g.has_path(1, 4));
    assert(!g.has_path(4, 1));
    assert(!g.has_path(2, 3));
    assert(!g.has_path(3, 2));

    // No cycles in a DAG
    for (int i = 1; i <= 4; ++i) {
        assert(!g.is_in_cycle(i));
    }
}

int main() {
    test_long_chain();
    test_multiple_independent_cycles();
    test_add_remove_churn();
    test_self_loops();
    test_out_of_range_ids();
    test_diamond_graph();
    return 0;
}
