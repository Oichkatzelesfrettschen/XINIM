#pragma once
/**
 * @file service.hpp
 * @brief Simple service manager for restarting crashed services.
 */

#include "../include/xinim/core_types.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sched {
class Scheduler;
} // namespace sched

namespace svc {

/**
 * @brief Manages kernel service processes and their dependencies.
 *
 * The manager stores a dependency DAG between services. When a service
 * crashes, the manager restarts it along with any dependents.
 */
class ServiceManager {
  public:
    /**
     * @brief Register a service with optional dependencies.
     *
     * @param pid Identifier of the service process.
     * @param deps List of services @p pid depends on.
     */
    void register_service(xinim::pid_t pid, const std::vector<xinim::pid_t> &deps = {});

    /**
     * @brief Restart a crashed service and its dependents.
     *
     * @param pid Identifier of the crashed service.
     */
    void handle_crash(xinim::pid_t pid);

  private:
    struct ServiceInfo {
        bool running{false};            ///< Whether the service is active
        std::vector<xinim::pid_t> deps; ///< Services this one depends on
    };

    bool has_path(xinim::pid_t start, xinim::pid_t target,
                  std::unordered_set<xinim::pid_t> &visited) const;
    void restart_tree(xinim::pid_t pid, std::unordered_set<xinim::pid_t> &visited);

    std::unordered_map<xinim::pid_t, ServiceInfo> services_{}; ///< Registered services
};

/// Global instance used by the kernel.
extern ServiceManager service_manager;

} // namespace svc
