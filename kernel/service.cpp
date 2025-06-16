#include "service.hpp"
#include "schedule.hpp"

#include <algorithm>
#include <ranges>

namespace svc {

/**
 * @brief Determine whether a dependency path exists between two services.
 *
 * The search performs a depth-first traversal while avoiding cycles using the
 * @p visited set.
 */
bool ServiceManager::has_path(xinim::pid_t start, xinim::pid_t target,
                              std::unordered_set<xinim::pid_t> &visited) const {
    if (start == target) {
        return true;
    }
    if (!visited.insert(start).second) {
        return false;
    }
    auto it = services_.find(start);
    if (it == services_.end()) {
        return false;
    }
    for (xinim::pid_t dep : it->second.deps) {
        if (has_path(dep, target, visited)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Register a new service with dependency information.
 *
 * Contracts are created lazily for each service and track restart counts.
 */
void ServiceManager::register_service(xinim::pid_t pid, const std::vector<xinim::pid_t> &deps) {
    auto &info = services_[pid];
    if (info.contract.id == 0) {
        info.contract.id = next_contract_id_++;
    }

    for (auto dep : deps) {
        std::unordered_set<xinim::pid_t> visited;
        if (!has_path(dep, pid, visited)) {
            info.deps.push_back(dep);
        }
    }

    info.running = true;
    sched::scheduler.enqueue(pid);
}

/**
 * @brief Restart @p pid and recursively restart dependents.
 */
void ServiceManager::restart_tree(xinim::pid_t pid, std::unordered_set<xinim::pid_t> &visited) {
    if (!visited.insert(pid).second) {
        return;
    }

    auto it = services_.find(pid);
    if (it == services_.end()) {
        return;
    }

    it->second.running = true;
    ++it->second.contract.count;
    sched::scheduler.enqueue(pid);

    for (auto &[other_pid, info] : services_) {
        if (std::ranges::contains(info.deps, pid)) {
            restart_tree(other_pid, visited);
        }
    }
}

/**
 * @brief React to a service crash by marking it inactive and restarting.
 */
void ServiceManager::handle_crash(xinim::pid_t pid) {
    auto it = services_.find(pid);
    if (it == services_.end()) {
        return;
    }
    it->second.running = false;
    std::unordered_set<xinim::pid_t> visited;
    restart_tree(pid, visited);
}

ServiceManager service_manager{};

std::atomic_uint64_t ServiceManager::next_contract_id_{1};

} // namespace svc
