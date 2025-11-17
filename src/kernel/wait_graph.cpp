#include "wait_graph.hpp"
#include <algorithm>
#include <functional>

namespace lattice {

bool WaitForGraph::has_path(xinim::pid_t from, xinim::pid_t to,
                            std::unordered_set<xinim::pid_t> &visited) const {
    if (from == to) {
        return true;
    }
    if (!visited.insert(from).second) {
        return false;
    }
    auto it = edges_.find(from);
    if (it == edges_.end()) {
        return false;
    }
    for (xinim::pid_t next : it->second) {
        if (has_path(next, to, visited)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Insert a wait dependency edge.
 *
 * @pre Both @p src and @p dst refer to valid process identifiers.
 * @post Detects cycles and rolls back insertion if a cycle would form.
 * @warning Current implementation is O(E); consider incremental SCC.
 */
bool WaitForGraph::add_edge(xinim::pid_t src, xinim::pid_t dst) {
    edges_[src].push_back(dst);
    std::unordered_set<xinim::pid_t> visited;
    if (has_path(dst, src, visited)) {
        auto &vec = edges_[src];
        vec.erase(std::find(vec.begin(), vec.end(), dst));
        return true;
    }
    return false;
}

/**
 * @brief Insert a typed dependency edge with metadata.
 *
 * This method supports IPC, RESOURCE, and LOCK edge types.
 */
bool WaitForGraph::add_edge(const Edge &edge) {
    // Add to basic graph structure
    edges_[edge.from].push_back(edge.to);

    // Store metadata
    edge_metadata_.push_back(edge);

    // Check for cycles
    std::unordered_set<xinim::pid_t> visited;
    if (has_path(edge.to, edge.from, visited)) {
        // Cycle detected! Roll back
        auto &vec = edges_[edge.from];
        vec.erase(std::find(vec.begin(), vec.end(), edge.to));
        edge_metadata_.pop_back();
        return true;
    }

    return false;
}

/**
 * @brief Add a lock wait edge to the graph.
 *
 * When a process waits for a lock, this creates a dependency edge in the
 * wait-for graph. If adding the edge creates a cycle, it indicates a deadlock.
 */
bool WaitForGraph::add_lock_edge(xinim::pid_t waiter, xinim::pid_t holder,
                                 void *lock_addr) {
    Edge edge;
    edge.from = waiter;
    edge.to = holder;
    edge.type = EdgeType::LOCK;
    edge.resource = lock_addr;
    edge.timestamp = 0; // TODO: Get monotonic time from kernel

    return add_edge(edge);
}

void WaitForGraph::remove_edge(xinim::pid_t src, xinim::pid_t dst) noexcept {
    auto it = edges_.find(src);
    if (it == edges_.end()) {
        return;
    }
    auto &vec = it->second;
    vec.erase(std::remove(vec.begin(), vec.end(), dst), vec.end());
    if (vec.empty()) {
        edges_.erase(it);
    }

    // Remove from metadata
    edge_metadata_.erase(
        std::remove_if(edge_metadata_.begin(), edge_metadata_.end(),
            [src, dst](const Edge &e) {
                return e.from == src && e.to == dst;
            }),
        edge_metadata_.end()
    );
}

/**
 * @brief Remove a lock wait edge.
 *
 * Called when a process acquires a lock it was waiting for, or gives up waiting.
 */
void WaitForGraph::remove_lock_edge(xinim::pid_t waiter, void *lock_addr) noexcept {
    // Find and remove the edge with matching waiter and lock address
    auto it = std::find_if(edge_metadata_.begin(), edge_metadata_.end(),
        [waiter, lock_addr](const Edge &e) {
            return e.from == waiter &&
                   e.type == EdgeType::LOCK &&
                   e.resource == lock_addr;
        });

    if (it != edge_metadata_.end()) {
        xinim::pid_t holder = it->to;
        edge_metadata_.erase(it);

        // Remove from basic graph
        remove_edge(waiter, holder);
    }
}

void WaitForGraph::clear(xinim::pid_t pid) noexcept {
    edges_.erase(pid);
    for (auto &pair : edges_) {
        auto &vec = pair.second;
        vec.erase(std::remove(vec.begin(), vec.end(), pid), vec.end());
    }

    // Clear from metadata
    edge_metadata_.erase(
        std::remove_if(edge_metadata_.begin(), edge_metadata_.end(),
            [pid](const Edge &e) {
                return e.from == pid || e.to == pid;
            }),
        edge_metadata_.end()
    );
}

/**
 * @brief Get all processes waiting on a given process.
 *
 * This is useful for finding which processes will be unblocked when @p pid
 * releases resources or crashes.
 */
std::vector<xinim::pid_t> WaitForGraph::get_waiters(xinim::pid_t pid) const {
    std::vector<xinim::pid_t> waiters;

    for (const auto &[from, to_list] : edges_) {
        for (xinim::pid_t to : to_list) {
            if (to == pid) {
                waiters.push_back(from);
            }
        }
    }

    return waiters;
}

/**
 * @brief Extract the cycle containing a given node.
 *
 * Performs DFS to find and extract the cycle. Returns empty vector if no cycle exists.
 */
std::vector<xinim::pid_t> WaitForGraph::extract_cycle(xinim::pid_t node) const {
    std::vector<xinim::pid_t> path;
    std::unordered_set<xinim::pid_t> visited;
    std::unordered_set<xinim::pid_t> rec_stack;

    // Helper lambda for DFS cycle detection
    std::function<bool(xinim::pid_t)> dfs = [&](xinim::pid_t current) -> bool {
        if (rec_stack.find(current) != rec_stack.end()) {
            // Found cycle! Extract it from path
            auto cycle_start = std::find(path.begin(), path.end(), current);
            if (cycle_start != path.end()) {
                path.erase(path.begin(), cycle_start);
            }
            return true;
        }

        if (visited.find(current) != visited.end()) {
            return false;
        }

        visited.insert(current);
        rec_stack.insert(current);
        path.push_back(current);

        auto it = edges_.find(current);
        if (it != edges_.end()) {
            for (xinim::pid_t next : it->second) {
                if (dfs(next)) {
                    return true;
                }
            }
        }

        rec_stack.erase(current);
        path.pop_back();
        return false;
    };

    if (dfs(node)) {
        return path;
    }

    return {};
}

} // namespace lattice
