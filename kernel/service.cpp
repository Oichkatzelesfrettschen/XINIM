#include "service.hpp"
#include "schedule.hpp"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
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
void ServiceManager::register_service(xinim::pid_t pid, const std::vector<xinim::pid_t> &deps,
                                      std::uint32_t limit) {
    auto &info = services_[pid];
    if (info.contract.id == 0) {
        info.contract.id = next_contract_id_++;
    }
    info.contract.policy.limit = limit;

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
 * @brief Add a new dependency to an existing service.
 *
 * The method ensures the dependency graph remains acyclic by validating that
 * adding @p dep does not introduce a path back to @p pid.
 *
 * @param pid Service that will gain the dependency.
 * @param dep Service that @p pid depends on.
 */
void ServiceManager::add_dependency(xinim::pid_t pid, xinim::pid_t dep) {
    auto it = services_.find(pid);
    if (it == services_.end()) {
        return;
    }

    std::unordered_set<xinim::pid_t> visited;
    if (!has_path(dep, pid, visited)) {
        it->second.deps.push_back(dep);
    }
}

/**
 * @brief Remove an existing dependency from a service.
 *
 * If either the service or dependency is unknown the call has no effect.
 */
void ServiceManager::remove_dependency(xinim::pid_t pid, xinim::pid_t dep) {
    auto it = services_.find(pid);
    if (it == services_.end()) {
        return;
    }
    std::erase(it->second.deps, dep);
}

/**
 * @brief Update the automatic restart limit for a service.
 *
 * @param pid   Service identifier to update.
 * @param limit New restart limit applied to the service contract.
 */
void ServiceManager::set_restart_limit(xinim::pid_t pid, std::uint32_t limit) {
    auto it = services_.find(pid);
    if (it == services_.end()) {
        return;
    }
    it->second.contract.policy.limit = limit;
}

/**
 * @brief Unregister a service and clean dependency references.
 *
 * Any dependency edges pointing to the service are removed so the DAG remains
 * consistent.
 */
void ServiceManager::unregister_service(xinim::pid_t pid) {
    if (!services_.erase(pid)) {
        return;
    }

    for (auto &[other_pid, info] : services_) {
        std::erase(info.deps, pid);
    }
}

/**
 * @brief Restart @p pid and recursively restart dependents.
 */
void ServiceManager::restart_tree(xinim::pid_t pid, std::unordered_set<xinim::pid_t> &visited) {
    auto it = services_.find(pid);
    if (it == services_.end()) {
        return; // skip services removed from the manager
    }

    if (!visited.insert(pid).second) {
        return;
    }

    auto &info = it->second;
    info.running = true;
    ++info.contract.restarts;
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
bool ServiceManager::handle_crash(xinim::pid_t pid) {
    auto it = services_.find(pid);
    if (it == services_.end()) {
        return false;
    }

    auto &info = it->second;
    info.running = false;

    if (info.contract.policy.limit != 0 && info.contract.restarts >= info.contract.policy.limit) {
        return false;
    }

    std::unordered_set<xinim::pid_t> visited;
    restart_tree(pid, visited);
    return true;
}

/**
 * @brief Retrieve the liveness contract for @p pid.
 *
 * If the service has not been registered, a static empty contract is returned
 * instead.
 *
 * @param pid Identifier of the service.
 * @return Reference to the associated contract or an empty fallback.
 */
const ServiceManager::LivenessContract &ServiceManager::contract(xinim::pid_t pid) const {
    static const LivenessContract empty{};
    if (auto it = services_.find(pid); it != services_.end()) {
        return it->second.contract;
    }
    return empty;
}

/**
 * @brief Check whether a service is currently running.
 *
 * @param pid Identifier of the service.
 * @return @c true if the service is active.
 */
bool ServiceManager::is_running(xinim::pid_t pid) const noexcept {
    if (auto it = services_.find(pid); it != services_.end()) {
        return it->second.running;
    }
    return false;
}

/**
 * @brief Construct the service manager and restore persisted configuration.
 */
ServiceManager::ServiceManager() { load(); }

/**
 * @brief Persist configuration when the manager is destroyed.
 */
ServiceManager::~ServiceManager() { save(); }

/**
 * @brief Serialize the service map to a JSON file.
 */
void ServiceManager::save(std::string_view path) const {
    nlohmann::json root;
    for (const auto &[pid, info] : services_) {
        root["services"].push_back({{"pid", pid},
                                    {"running", info.running},
                                    {"deps", info.deps},
                                    {"contract",
                                     {{"id", info.contract.id},
                                      {"limit", info.contract.policy.limit},
                                      {"restarts", info.contract.restarts}}}});
    }
    std::ofstream out{std::string{path}};
    if (out) {
        out << root.dump(2);
    }
}

/**
 * @brief Load a service map from a JSON file.
 */
void ServiceManager::load(std::string_view path) {
    services_.clear();
    std::ifstream in{std::string{path}};
    if (!in) {
        return;
    }
    nlohmann::json root;
    in >> root;
    for (const auto &svc : root["services"]) {
        ServiceInfo info;
        xinim::pid_t pid = svc["pid"].get<xinim::pid_t>();
        info.running = svc["running"].get<bool>();
        info.deps = svc["deps"].get<std::vector<xinim::pid_t>>();
        info.contract.id = svc["contract"]["id"].get<std::uint64_t>();
        info.contract.policy.limit = svc["contract"]["limit"].get<std::uint32_t>();
        info.contract.restarts = svc["contract"]["restarts"].get<std::uint32_t>();
        services_.emplace(pid, std::move(info));
    }

    next_contract_id_ = 1;
    for (const auto &[_, info] : services_) {
        auto next = info.contract.id + 1;
        next_contract_id_ = std::max(next_contract_id_.load(), next);
    }
}

/// Global manager instance accessible throughout the kernel.
ServiceManager service_manager{};

/// Counter used to assign unique IDs to liveness contracts.
std::atomic_uint64_t ServiceManager::next_contract_id_{1};

} // namespace svc
