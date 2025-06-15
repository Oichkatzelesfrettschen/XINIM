#include "service.hpp"
#include "schedule.hpp"

namespace svc {

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

void ServiceManager::register_service(xinim::pid_t pid, const std::vector<xinim::pid_t> &deps) {
    auto &info = services_[pid];
    for (xinim::pid_t dep : deps) {
        std::unordered_set<xinim::pid_t> visited;
        if (!has_path(dep, pid, visited)) {
            info.deps.push_back(dep);
        }
    }
    info.running = true;
    sched::scheduler.enqueue(pid);
}

void ServiceManager::restart_tree(xinim::pid_t pid, std::unordered_set<xinim::pid_t> &visited) {
    if (!visited.insert(pid).second) {
        return;
    }
    auto it = services_.find(pid);
    if (it == services_.end()) {
        return;
    }
    it->second.running = true;
    sched::scheduler.enqueue(pid);
    for (auto &[other_pid, info] : services_) {
        if (std::find(info.deps.begin(), info.deps.end(), pid) != info.deps.end()) {
            restart_tree(other_pid, visited);
        }
    }
}

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

} // namespace svc
