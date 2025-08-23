#pragma once
/**
 * @file wait_graph.hpp
 * @brief Directed wait-for graph for detecting process deadlocks.
 */

#include "../include/xinim/core_types.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lattice {

/**
 * @brief Wait-for graph manager storing blocking relationships.
 */
class WaitForGraph {
  public:
    /**
     * @brief Add a dependency edge from @p src to @p dst.
     *
     * The function detects if inserting the edge would create a cycle. If so,
     * the edge is not retained and the function returns @c true.
     *
     * @param src Process waiting.
     * @param dst Process being waited on.
     * @return @c true if a cycle was detected.
     */
    [[nodiscard]] bool add_edge(xinim::pid_t src, xinim::pid_t dst);

    /**
     * @brief Remove the dependency edge @p src -> @p dst if present.
     */
    void remove_edge(xinim::pid_t src, xinim::pid_t dst) noexcept;

    /**
     * @brief Remove all edges originating from or targeting @p pid.
     */
    void clear(xinim::pid_t pid) noexcept;

  private:
    bool has_path(xinim::pid_t from, xinim::pid_t to,
                  std::unordered_set<xinim::pid_t> &visited) const;

    std::unordered_map<xinim::pid_t, std::vector<xinim::pid_t>> edges_{};
};

} // namespace lattice
