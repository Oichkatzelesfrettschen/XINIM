#pragma once
/**
 * @file wait_graph.hpp
 * @brief Directed wait-for graph for detecting process deadlocks (Fixed-size).
 */

#include "../include/xinim/core_types.hpp"

#include <array>
#include <span>

namespace lattice {

    /**
     * @brief Represents an edge in the wait-for graph.
     */
    struct Edge {
        xinim::pid_t from;
        xinim::pid_t to;
    };

    /**
     * @brief Wait-for graph implementation using fixed-size adjacency matrix.
     */
    class WaitForGraph {
    public:
        static constexpr int MAX_NODES = 64;

        WaitForGraph() {
            for (auto &row : adj_)
                row.fill(false);
        }

        /** @brief Add a directed edge from @p from to @p to. */
        void add_edge(xinim::pid_t from, xinim::pid_t to) noexcept;

        /** @brief Remove the directed edge from @p from to @p to. */
        void remove_edge(xinim::pid_t from, xinim::pid_t to) noexcept;

        /** @brief Remove every incoming and outgoing edge for @p node. */
        void remove_node(xinim::pid_t node) noexcept;

        /** @brief Check if there is a path from @p from to @p to. */
        bool has_path(xinim::pid_t from, xinim::pid_t to) const noexcept;

        /** @brief Check if @p node is part of a cycle. */
        bool is_in_cycle(xinim::pid_t node) const noexcept;

    private:
        std::array<std::array<bool, MAX_NODES>, MAX_NODES> adj_{};
    };

} // namespace lattice
