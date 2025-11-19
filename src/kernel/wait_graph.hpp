#pragma once
/**
 * @file wait_graph.hpp
 * @brief Directed wait-for graph for detecting process deadlocks.
 */

#include "../include/xinim/core_types.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

namespace lattice {

/**
 * @brief Type of dependency edge in the wait-for graph.
 */
enum class EdgeType : std::uint8_t {
    IPC,      ///< Process waiting on IPC message
    RESOURCE, ///< Process waiting on resource allocation
    LOCK      ///< Process waiting on lock acquisition (NEW)
};

/**
 * @brief Edge metadata describing a wait dependency.
 */
struct Edge {
    xinim::pid_t from;      ///< Process that is waiting
    xinim::pid_t to;        ///< Process being waited on (lock holder)
    EdgeType type;          ///< Type of dependency
    void *resource;         ///< Resource pointer (lock address for LOCK type)
    std::uint64_t timestamp; ///< When the wait started (monotonic time)
};

/**
 * @brief Wait-for graph manager storing blocking relationships.
 *
 * Enhanced to support different edge types including LOCK edges for
 * deadlock detection across the lock framework.
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
     * @brief Add a typed dependency edge with metadata.
     *
     * @param edge Edge containing dependency information.
     * @return @c true if a cycle was detected.
     */
    [[nodiscard]] bool add_edge(const Edge &edge);

    /**
     * @brief Add a lock wait edge to the graph.
     *
     * This specialized method adds an edge indicating that @p waiter is blocked
     * waiting for a lock currently held by @p holder.
     *
     * @param waiter Process waiting for the lock.
     * @param holder Process holding the lock.
     * @param lock_addr Address of the lock object.
     * @return @c true if adding this edge creates a deadlock cycle.
     */
    [[nodiscard]] bool add_lock_edge(xinim::pid_t waiter, xinim::pid_t holder,
                                     void *lock_addr);

    /**
     * @brief Remove the dependency edge @p src -> @p dst if present.
     */
    void remove_edge(xinim::pid_t src, xinim::pid_t dst) noexcept;

    /**
     * @brief Remove a lock wait edge.
     *
     * @param waiter Process that was waiting.
     * @param lock_addr Address of the lock.
     */
    void remove_lock_edge(xinim::pid_t waiter, void *lock_addr) noexcept;

    /**
     * @brief Remove all edges originating from or targeting @p pid.
     */
    void clear(xinim::pid_t pid) noexcept;

    /**
     * @brief Get all processes waiting on @p pid.
     *
     * @param pid Process identifier.
     * @return Vector of processes waiting on @p pid.
     */
    [[nodiscard]] std::vector<xinim::pid_t> get_waiters(xinim::pid_t pid) const;

    /**
     * @brief Extract the cycle containing @p node if one exists.
     *
     * @param node Node suspected to be in a cycle.
     * @return Vector of PIDs forming the cycle, empty if no cycle.
     */
    [[nodiscard]] std::vector<xinim::pid_t> extract_cycle(xinim::pid_t node) const;

  private:
    bool has_path(xinim::pid_t from, xinim::pid_t to,
                  std::unordered_set<xinim::pid_t> &visited) const;

    std::unordered_map<xinim::pid_t, std::vector<xinim::pid_t>> edges_{};
    std::vector<Edge> edge_metadata_{}; ///< Extended edge information
};

} // namespace lattice
