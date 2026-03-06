/**
 * @file recovery_dag.cpp
 * @brief RecoveryDag implementation -- DAG-based service restart ordering.
 */

#include "recovery_dag.hpp"

namespace xinim::kernel::recovery {

int RecoveryDag::add_service(const char* name, RestartPolicy policy, uint8_t max_restarts) {
    if (count_ >= MAX_SERVICES) return -1;

    int idx = static_cast<int>(count_);
    services_[idx].set_name(name);
    services_[idx].policy = policy;
    services_[idx].max_restarts = max_restarts;
    services_[idx].state = ServiceState::STOPPED;
    services_[idx].restart_count = 0;
    services_[idx].pid = -1;
    services_[idx].ndeps = 0;
    count_++;
    return idx;
}

bool RecoveryDag::add_dependency(int dependent, int dependency) {
    if (dependent < 0 || dependent >= static_cast<int>(count_)) return false;
    if (dependency < 0 || dependency >= static_cast<int>(count_)) return false;
    if (dependent == dependency) return false;

    ServiceNode& node = services_[dependent];
    if (node.ndeps >= MAX_DEPS) return false;

    // Check for duplicate
    for (uint8_t i = 0; i < node.ndeps; i++) {
        if (node.deps[i] == static_cast<int16_t>(dependency)) return true; // already exists
    }

    // Check if adding this edge would create a cycle
    if (would_create_cycle(dependent, dependency)) return false;

    node.deps[node.ndeps++] = static_cast<int16_t>(dependency);
    return true;
}

int RecoveryDag::find_service(const char* name) const {
    for (size_t i = 0; i < count_; i++) {
        if (services_[i].name_matches(name)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int RecoveryDag::find_by_pid(int32_t pid) const {
    for (size_t i = 0; i < count_; i++) {
        if (services_[i].pid == pid) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void RecoveryDag::set_pid(int idx, int32_t pid) {
    if (idx >= 0 && idx < static_cast<int>(count_)) {
        services_[idx].pid = pid;
    }
}

void RecoveryDag::set_running(int idx) {
    if (idx >= 0 && idx < static_cast<int>(count_)) {
        services_[idx].state = ServiceState::RUNNING;
    }
}

int RecoveryDag::notify_crash(int idx, int restart_order[], int max_order) {
    if (idx < 0 || idx >= static_cast<int>(count_)) return 0;

    ServiceNode& crashed = services_[idx];
    crashed.state = ServiceState::CRASHED;
    crashed.pid = -1;
    crashed.restart_count++;

    switch (crashed.policy) {
    case RestartPolicy::IGNORE:
        return 0;

    case RestartPolicy::PANIC:
        // Caller should kernel panic
        return -1;

    case RestartPolicy::RESTART:
        // Just restart this one service
        if (max_order >= 1) {
            restart_order[0] = idx;
            return 1;
        }
        return 0;

    case RestartPolicy::KILL_DEPS:
        // Find all services that transitively depend on the crashed one
        // and restart them in topological order (dependencies first)
        return topo_sort_dependents(idx, restart_order, max_order);
    }

    return 0;
}

bool RecoveryDag::validate() const {
    // Kahn's algorithm on the full graph to detect cycles.
    // Edge: dependent -> dependency (dependent has deps[] listing its prerequisites).
    // In-degree = number of prerequisites (deps) a node has.
    // Kahn processes nodes with zero in-degree (no prerequisites) first.
    int16_t in_deg[MAX_SERVICES];
    for (size_t i = 0; i < count_; i++) {
        in_deg[i] = static_cast<int16_t>(services_[i].ndeps);
    }

    // Start with nodes that have no dependencies
    int queue[MAX_SERVICES];
    int qhead = 0, qtail = 0;
    for (size_t i = 0; i < count_; i++) {
        if (in_deg[i] == 0) {
            queue[qtail++] = static_cast<int>(i);
        }
    }

    int processed = 0;
    while (qhead < qtail) {
        int node = queue[qhead++];
        processed++;

        // For each service that lists 'node' as a dependency,
        // decrement its in-degree
        for (size_t i = 0; i < count_; i++) {
            for (uint8_t j = 0; j < services_[i].ndeps; j++) {
                if (services_[i].deps[j] == static_cast<int16_t>(node)) {
                    in_deg[i]--;
                    if (in_deg[i] == 0) {
                        queue[qtail++] = static_cast<int>(i);
                    }
                }
            }
        }
    }

    return processed == static_cast<int>(count_);
}

const ServiceNode* RecoveryDag::get_service(int idx) const {
    if (idx >= 0 && idx < static_cast<int>(count_)) {
        return &services_[idx];
    }
    return nullptr;
}

ServiceNode* RecoveryDag::get_service_mut(int idx) {
    if (idx >= 0 && idx < static_cast<int>(count_)) {
        return &services_[idx];
    }
    return nullptr;
}

int RecoveryDag::topo_sort_dependents(int root, int order[], int max_order) const {
    // Find all services that transitively depend on root (reverse DFS),
    // then topological sort them (root first, then dependents in dependency order).

    // Mark which services are affected (root + all reverse-reachable)
    bool affected[MAX_SERVICES] = {};
    affected[root] = true;

    // BFS to find all services that depend on root (directly or transitively)
    int bfs[MAX_SERVICES];
    int bhead = 0, btail = 0;
    bfs[btail++] = root;

    while (bhead < btail) {
        int node = bfs[bhead++];
        // Find services whose deps include 'node'
        for (size_t i = 0; i < count_; i++) {
            if (affected[i]) continue;
            for (uint8_t j = 0; j < services_[i].ndeps; j++) {
                if (services_[i].deps[j] == static_cast<int16_t>(node)) {
                    affected[i] = true;
                    bfs[btail++] = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    // Topological sort of affected services using Kahn's algorithm
    // Dependencies come first in the restart order
    int16_t in_deg[MAX_SERVICES] = {};

    // Compute in-degrees only among affected services
    for (size_t i = 0; i < count_; i++) {
        if (!affected[i]) continue;
        for (uint8_t j = 0; j < services_[i].ndeps; j++) {
            int dep = services_[i].deps[j];
            if (dep >= 0 && dep < static_cast<int>(count_) && affected[dep]) {
                in_deg[i]++;
            }
        }
    }

    // Kahn: start with zero in-degree affected nodes
    int queue[MAX_SERVICES];
    int qhead = 0, qtail = 0;
    for (size_t i = 0; i < count_; i++) {
        if (affected[i] && in_deg[i] == 0) {
            queue[qtail++] = static_cast<int>(i);
        }
    }

    int out_count = 0;
    while (qhead < qtail && out_count < max_order) {
        int node = queue[qhead++];
        order[out_count++] = node;

        // Reduce in-degrees of dependents
        for (size_t i = 0; i < count_; i++) {
            if (!affected[i]) continue;
            for (uint8_t j = 0; j < services_[i].ndeps; j++) {
                if (services_[i].deps[j] == static_cast<int16_t>(node)) {
                    in_deg[i]--;
                    if (in_deg[i] == 0) {
                        queue[qtail++] = static_cast<int>(i);
                    }
                }
            }
        }
    }

    return out_count;
}

bool RecoveryDag::would_create_cycle(int from, int to) const {
    // Adding edge from->to (from depends on to).
    // A cycle exists if 'from' is already reachable from 'to' via deps.
    return can_reach(to, from);
}

bool RecoveryDag::can_reach(int start, int target) const {
    // DFS: can we reach 'target' from 'start' following dependency edges?
    if (start == target) return true;

    bool visited[MAX_SERVICES] = {};
    int stack[MAX_SERVICES];
    int sp = 0;
    stack[sp++] = start;
    visited[start] = true;

    while (sp > 0) {
        int node = stack[--sp];
        for (uint8_t i = 0; i < services_[node].ndeps; i++) {
            int dep = services_[node].deps[i];
            if (dep == target) return true;
            if (dep >= 0 && dep < static_cast<int>(count_) && !visited[dep]) {
                visited[dep] = true;
                stack[sp++] = dep;
            }
        }
    }
    return false;
}

} // namespace xinim::kernel::recovery
