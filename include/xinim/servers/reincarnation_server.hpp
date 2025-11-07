/**
 * @file reincarnation_server.hpp
 * @brief MINIX-style Reincarnation Server for Driver Fault Recovery
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * Based on MINIX 3 Reincarnation Server (RS) design:
 * - Automatic fault detection in drivers and servers
 * - Policy-driven recovery and restart
 * - Transparent recovery for user processes
 * - Process hierarchy management
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <unordered_map>

namespace xinim::servers {

/**
 * @brief Service Types
 */
enum class ServiceType {
    DRIVER,        // Device driver
    SERVER,        // System server (VFS, PM, etc.)
    TASK,          // Kernel task
    USER_PROCESS,  // User-mode process
};

/**
 * @brief Service State
 */
enum class ServiceState {
    DEAD,          // Not running
    STARTING,      // Being started
    RUNNING,       // Active and healthy
    STOPPING,      // Being stopped
    CRASHED,       // Fault detected
    RECOVERING,    // Being restarted
    FAILED,        // Recovery failed
};

/**
 * @brief Recovery Policy
 */
struct RecoveryPolicy {
    uint32_t max_retries = 5;              // Maximum restart attempts
    std::chrono::seconds retry_interval{5}; // Delay between retries
    bool auto_restart = true;               // Automatic restart on crash
    bool preserve_state = false;            // Attempt state restoration
    bool notify_dependents = true;          // Notify dependent services
    
    // Escalation policy
    enum class EscalationAction {
        NONE,           // Just log the failure
        RESTART_DEPS,   // Restart dependent services
        SYSTEM_ALERT,   // Alert system administrator
        SAFE_MODE,      // Enter safe mode
    };
    EscalationAction on_repeated_failure = EscalationAction::SYSTEM_ALERT;
};

/**
 * @brief Service Descriptor
 */
struct ServiceDescriptor {
    uint32_t service_id;
    std::string name;
    std::string executable_path;
    std::vector<std::string> args;
    ServiceType type;
    ServiceState state;
    
    // Process information
    int32_t pid;
    int32_t uid;
    int32_t gid;
    
    // Recovery tracking
    uint32_t crash_count;
    uint32_t restart_count;
    std::chrono::system_clock::time_point last_crash;
    std::chrono::system_clock::time_point last_restart;
    
    // Policy
    RecoveryPolicy policy;
    
    // Dependencies
    std::vector<uint32_t> depends_on;     // Services this depends on
    std::vector<uint32_t> depended_by;    // Services that depend on this
    
    // Health monitoring
    std::chrono::system_clock::time_point last_heartbeat;
    uint32_t heartbeat_timeout_ms = 5000;
    
    // State checkpoint (for stateful drivers)
    void* state_checkpoint = nullptr;
    size_t state_size = 0;
};

/**
 * @brief Reincarnation Server (RS)
 * 
 * Manages lifecycle of all drivers and servers:
 * - Starts and stops services
 * - Monitors health via heartbeats
 * - Detects crashes via SIGCHLD
 * - Automatically restarts failed services
 * - Manages service dependencies
 * - Provides transparent recovery
 */
class ReincarnationServer {
public:
    static ReincarnationServer& instance();
    
    // Service management
    uint32_t register_service(const std::string& name,
                              const std::string& executable,
                              const std::vector<std::string>& args,
                              ServiceType type,
                              const RecoveryPolicy& policy = RecoveryPolicy{});
    
    bool start_service(uint32_t service_id);
    bool stop_service(uint32_t service_id);
    bool restart_service(uint32_t service_id);
    
    // Dependency management
    bool add_dependency(uint32_t service_id, uint32_t depends_on_id);
    bool remove_dependency(uint32_t service_id, uint32_t depends_on_id);
    
    // Health monitoring
    void record_heartbeat(uint32_t service_id);
    void check_health();  // Called periodically
    
    // Crash handling
    void handle_crash(int32_t pid, int exit_status, int signal);
    
    // State management
    bool checkpoint_state(uint32_t service_id, const void* state, size_t size);
    bool restore_state(uint32_t service_id, void* state, size_t max_size);
    
    // Query
    ServiceState get_service_state(uint32_t service_id) const;
    ServiceDescriptor* get_service(uint32_t service_id);
    std::vector<uint32_t> get_all_services() const;
    std::vector<uint32_t> get_services_by_type(ServiceType type) const;
    
    // Statistics
    struct Statistics {
        uint32_t total_services;
        uint32_t running_services;
        uint32_t crashed_services;
        uint32_t total_crashes;
        uint32_t total_recoveries;
        uint32_t failed_recoveries;
    };
    Statistics get_statistics() const;

private:
    ReincarnationServer() = default;
    ~ReincarnationServer() = default;
    
    // No copy
    ReincarnationServer(const ReincarnationServer&) = delete;
    ReincarnationServer& operator=(const ReincarnationServer&) = delete;
    
    // Internal methods
    bool start_process(ServiceDescriptor& service);
    void terminate_process(ServiceDescriptor& service);
    bool can_recover(const ServiceDescriptor& service) const;
    void perform_recovery(ServiceDescriptor& service);
    void notify_dependencies(uint32_t service_id, ServiceState new_state);
    void escalate_failure(const ServiceDescriptor& service);
    
    // Service registry
    std::unordered_map<uint32_t, ServiceDescriptor> services_;
    std::unordered_map<int32_t, uint32_t> pid_to_service_;
    uint32_t next_service_id_ = 1;
    
    // Statistics
    Statistics stats_{};
};

/**
 * @brief Driver Framework Integration
 * 
 * Helper class for drivers to integrate with RS
 */
class ManagedDriver {
public:
    ManagedDriver(const std::string& name);
    virtual ~ManagedDriver();
    
    // Called by RS during startup
    virtual bool initialize() = 0;
    
    // Called by RS during shutdown
    virtual void shutdown() = 0;
    
    // Called by RS to checkpoint state
    virtual bool save_state(void* buffer, size_t max_size) = 0;
    
    // Called by RS to restore state
    virtual bool load_state(const void* buffer, size_t size) = 0;
    
    // Send heartbeat to RS
    void send_heartbeat();
    
protected:
    std::string driver_name_;
    uint32_t service_id_;
};

} // namespace xinim::servers
