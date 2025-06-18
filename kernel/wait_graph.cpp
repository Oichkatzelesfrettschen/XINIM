#include "wait_graph.hpp"
#include <algorithm>

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
}

void WaitForGraph::clear(xinim::pid_t pid) noexcept {
    edges_.erase(pid);
    for (auto &pair : edges_) {
        auto &vec = pair.second;
        vec.erase(std::remove(vec.begin(), vec.end(), pid), vec.end());
    }
}

} // namespace lattice
