#pragma once
/**
 * @file service.hpp
 * @brief Simple service manager for restarting crashed services.
 */

#include "../include/xinim/core_types.hpp"
#include <atomic>
#include <ranges>
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
     * @brief Restart policy describing allowed automatic restarts.
     */
    struct RestartPolicy {
        std::uint32_t limit{3}; ///< Maximum automatic restarts, 0 for unlimited
    };

    /**
     * @brief Contract tracking service liveness and restarts.
     */
    struct LivenessContract {
        std::uint64_t id{0};       ///< Unique contract identifier
        RestartPolicy policy{};    ///< Associated restart policy
        std::uint32_t restarts{0}; ///< Number of performed restarts
    };

    /**
     * @brief Register a service with optional dependencies and restart limit.
     *
     * @param pid   Identifier of the service process.
     * @param deps  List of services @p pid depends on.
     * @param limit Automatic restart limit for liveness enforcement.
     */
    void register_service(xinim::pid_t pid, const std::vector<xinim::pid_t> &deps = {},
                          std::uint32_t limit = 3);

    /**
     * @brief Declare an additional dependency after registration.
     *
     * @param pid  Service that depends on @p dep.
     * @param dep  Dependency to add.
     */
    void add_dependency(xinim::pid_t pid, xinim::pid_t dep);

    /**
     * @brief Change the restart limit for a service.
     *
     * @param pid   Service to update.
     * @param limit New restart limit.
     */
    void set_restart_limit(xinim::pid_t pid, std::uint32_t limit);

    /**
     * @brief Restart a crashed service and its dependents if allowed.
     *
     * @param pid Identifier of the crashed service.
     * @return @c true if the service was restarted.
     */
    [[nodiscard]] bool handle_crash(xinim::pid_t pid);

    /**
     * @brief Access the liveness contract for a service.
     *
     * @param pid Identifier of the service.
     * @return Reference to the contract.
     */
    [[nodiscard]] const LivenessContract &contract(xinim::pid_t pid) const;

    /**
     * @brief Query whether a service is currently running.
     *
     * @param pid Identifier of the service.
     * @return @c true if running.
     */
    [[nodiscard]] bool is_running(xinim::pid_t pid) const noexcept;

  private:
    /**
     * @brief Metadata associated with each registered service.
     */
    struct ServiceInfo {
        bool running{false};            ///< Whether the service is active
        std::vector<xinim::pid_t> deps; ///< Services this one depends on
        LivenessContract contract{};    ///< Liveness contract for restarts
    };

    /**
     * @brief Check if a dependency path exists from @p start to @p target.
     *
     * The search avoids cycles by keeping track of previously @p visited
     * services. A true result indicates that @p start transitively depends on
     * @p target.
     *
     * @param start   Service to begin the search from.
     * @param target  Service to reach.
     * @param visited Set of services already examined.
     * @return @c true if a path exists.
     */
    bool has_path(xinim::pid_t start, xinim::pid_t target,
                  std::unordered_set<xinim::pid_t> &visited) const;

    /**
     * @brief Restart @p pid and recursively restart all dependents.
     *
     * This helper traverses the dependency DAG and restarts every service that
     * directly or indirectly depends on @p pid. The @p visited set prevents
     * cycles during the traversal.
     *
     * @param pid     Service to restart.
     * @param visited Set of services already restarted in this operation.
     */
    void restart_tree(xinim::pid_t pid, std::unordered_set<xinim::pid_t> &visited);

    std::unordered_map<xinim::pid_t, ServiceInfo> services_{}; ///< Registered services
    static std::atomic_uint64_t next_contract_id_;             ///< Counter for contract IDs
};

/// Global instance used by the kernel.
extern ServiceManager service_manager;

} // namespace svc
