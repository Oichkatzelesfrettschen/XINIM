/**
 * @file recovery_dag.hpp
 * @brief DAG-based service dependency tracker for the Resurrection Server.
 *
 * Maintains a directed acyclic graph of service dependencies.
 * On service crash, computes a topological restart order using
 * Kahn's algorithm.
 *
 * Inspired by Minix 3.4.0 servers/rs/ -- ported natively to C++23.
 * Freestanding -- no STL.
 */
#pragma once

#include "service_node.hpp"

namespace xinim::kernel::recovery {

static constexpr size_t MAX_SERVICES = 32;

class RecoveryDag {
public:
    RecoveryDag() = default;

    // Register a new service. Returns index, or -1 on full.
    int add_service(const char* name, RestartPolicy policy, uint8_t max_restarts = 5);

    // Add dependency: 'dependent' depends on 'dependency'.
    // Both are indices from add_service().
    // Returns true on success, false if invalid or would create cycle.
    bool add_dependency(int dependent, int dependency);

    // Look up service by name. Returns index or -1.
    int find_service(const char* name) const;

    // Look up service by PID. Returns index or -1.
    int find_by_pid(int32_t pid) const;

    // Set the PID of a running service.
    void set_pid(int idx, int32_t pid);

    // Mark service as running.
    void set_running(int idx);

    // Set the service state explicitly.
    void set_state(int idx, ServiceState state);

    // Notify that a service has crashed.
    // Returns the number of services that need restarting.
    // Fills 'restart_order' with indices in topological restart order.
    int notify_crash(int idx, int restart_order[], int max_order);

    // Check for cycles in the graph. Returns true if DAG is valid (no cycles).
    bool validate() const;

    // Access service node by index.
    const ServiceNode* get_service(int idx) const;
    ServiceNode* get_service_mut(int idx);

    size_t service_count() const { return count_; }

private:
    ServiceNode services_[MAX_SERVICES];
    size_t count_{0};

    // Kahn's algorithm: topological sort of services that depend on 'root'.
    // Returns number of services placed in 'order'.
    int topo_sort_dependents(int root, int order[], int max_order) const;

    // Check if adding edge (from -> to) would create a cycle.
    bool would_create_cycle(int from, int to) const;

    // DFS reachability check: can 'target' reach 'start' via deps?
    bool can_reach(int start, int target) const;
};

} // namespace xinim::kernel::recovery
