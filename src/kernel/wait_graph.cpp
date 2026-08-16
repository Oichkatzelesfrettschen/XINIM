#include "wait_graph.hpp"

namespace lattice {

    void WaitForGraph::add_edge(xinim::pid_t from, xinim::pid_t to) noexcept {
        if (from >= 0 && from < MAX_NODES && to >= 0 && to < MAX_NODES) {
            const auto from_index = static_cast<std::size_t>(from);
            const auto to_index = static_cast<std::size_t>(to);
            adj_[from_index][to_index] = true;
        }
    }

    void WaitForGraph::remove_edge(xinim::pid_t from, xinim::pid_t to) noexcept {
        if (from >= 0 && from < MAX_NODES && to >= 0 && to < MAX_NODES) {
            const auto from_index = static_cast<std::size_t>(from);
            const auto to_index = static_cast<std::size_t>(to);
            adj_[from_index][to_index] = false;
        }
    }

    void WaitForGraph::remove_node(xinim::pid_t node) noexcept {
        if (node < 0 || node >= MAX_NODES)
            return;
        const auto node_index = static_cast<std::size_t>(node);
        for (std::size_t index = 0; index < adj_.size(); ++index) {
            adj_[node_index][index] = false;
            adj_[index][node_index] = false;
        }
    }

    bool WaitForGraph::has_path(xinim::pid_t from, xinim::pid_t to) const noexcept {
        if (from < 0 || from >= MAX_NODES || to < 0 || to >= MAX_NODES)
            return false;
        if (from == to)
            return true;

        xinim::pid_t queue[MAX_NODES];
        bool visited[MAX_NODES]{};
        std::size_t head = 0;
        std::size_t tail = 0;

        queue[tail++] = from;
        visited[static_cast<std::size_t>(from)] = true;

        while (head < tail) {
            const xinim::pid_t current = queue[head++];
            const auto current_index = static_cast<std::size_t>(current);
            for (std::size_t next_index = 0; next_index < adj_.size(); ++next_index) {
                if (adj_[current_index][next_index]) {
                    if (next_index == static_cast<std::size_t>(to))
                        return true;
                    if (!visited[next_index]) {
                        visited[next_index] = true;
                        queue[tail++] = static_cast<xinim::pid_t>(next_index);
                    }
                }
            }
        }

        return false;
    }

    bool WaitForGraph::is_in_cycle(xinim::pid_t node) const noexcept {
        if (node < 0 || node >= MAX_NODES)
            return false;
        const auto node_index = static_cast<std::size_t>(node);
        for (std::size_t next_index = 0; next_index < adj_.size(); ++next_index) {
            if (adj_[node_index][next_index]) {
                if (has_path(static_cast<xinim::pid_t>(next_index), node))
                    return true;
            }
        }
        return false;
    }

} // namespace lattice
