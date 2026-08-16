/**
 * @file test_wait_graph.cpp
 * @brief Unit tests for lattice::WaitForGraph deadlock detection.
 */

#include "wait_graph.hpp"

#include <cassert>

int main() {
    lattice::WaitForGraph g;

    // Add edges 1->2, 2->3 (no cycle)
    g.add_edge(1, 2);
    g.add_edge(2, 3);
    assert(g.has_path(1, 3));
    assert(!g.has_path(3, 1));
    assert(!g.is_in_cycle(1));

    // Adding 3->1 creates a cycle
    g.add_edge(3, 1);
    assert(g.has_path(3, 1));
    assert(g.is_in_cycle(1));
    assert(g.is_in_cycle(2));
    assert(g.is_in_cycle(3));

    // Remove edge breaks cycle
    g.remove_edge(3, 1);
    assert(!g.is_in_cycle(1));
    assert(!g.has_path(3, 1));

    // Path still exists 1->2->3
    assert(g.has_path(1, 3));

    // Removing a node clears both incoming and outgoing edges.
    g.remove_node(2);
    assert(!g.has_path(1, 3));
    assert(!g.has_path(1, 2));
    assert(!g.has_path(2, 3));

    // Self-loop is a cycle
    g.add_edge(5, 5);
    assert(g.is_in_cycle(5));

    // Out-of-range PIDs are handled gracefully
    g.add_edge(-1, 2);
    g.add_edge(100, 2);
    assert(!g.has_path(-1, 2));
    assert(!g.has_path(100, 2));

    return 0;
}
