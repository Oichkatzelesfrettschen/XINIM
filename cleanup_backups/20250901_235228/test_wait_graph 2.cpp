#include "../kernel/wait_graph.hpp"
#include <cassert>

int main() {
    lattice::WaitForGraph g;
    // Add edge 1->2
    assert(!g.add_edge(1, 2));
    // Add edge 2->3
    assert(!g.add_edge(2, 3));
    // Adding 3->1 should create a cycle
    assert(g.add_edge(3, 1));
    // Graph should not have kept the edge
    g.remove_edge(1, 2);
    g.clear(2);
    return 0;
}
