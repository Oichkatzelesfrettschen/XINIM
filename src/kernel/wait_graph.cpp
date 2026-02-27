#include "wait_graph.hpp"

namespace lattice {

void WaitForGraph::add_edge(xinim::pid_t from, xinim::pid_t to) noexcept {
    if (from >= 0 && from < MAX_NODES && to >= 0 && to < MAX_NODES) {
        adj_[from][to] = true;
    }
}

void WaitForGraph::remove_edge(xinim::pid_t from, xinim::pid_t to) noexcept {
    if (from >= 0 && from < MAX_NODES && to >= 0 && to < MAX_NODES) {
        adj_[from][to] = false;
    }
}

bool WaitForGraph::has_path(xinim::pid_t from, xinim::pid_t to) const noexcept {
    if (from < 0 || from >= MAX_NODES || to < 0 || to >= MAX_NODES) return false;
    if (from == to) return true;

    xinim::pid_t queue[MAX_NODES];
    bool visited[MAX_NODES]{};
    int head = 0, tail = 0;

    queue[tail++] = from;
    visited[from] = true;

    while (head < tail) {
        xinim::pid_t curr = queue[head++];
        for (int next = 0; next < MAX_NODES; ++next) {
            if (adj_[curr][next]) {
                if (next == to) return true;
                if (!visited[next]) {
                    visited[next] = true;
                    queue[tail++] = next;
                }
            }
        }
    }

    return false;
}

bool WaitForGraph::is_in_cycle(xinim::pid_t node) const noexcept {
    if (node < 0 || node >= MAX_NODES) return false;
    for (int next = 0; next < MAX_NODES; ++next) {
        if (adj_[node][next]) {
            if (has_path(next, node)) return true;
        }
    }
    return false;
}

} // namespace lattice
