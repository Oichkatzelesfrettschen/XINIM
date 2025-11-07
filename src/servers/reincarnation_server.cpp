/**
 * @file reincarnation_server.cpp
 * @brief MINIX Reincarnation Server implementation
 * 
 * Provides fault-tolerant driver and server management with automatic
 * crash detection and recovery. Based on MINIX 3 RS design.
 */

#include <xinim/servers/reincarnation_server.hpp>
#include <algorithm>
#include <chrono>

namespace xinim::servers {

ReincarnationServer::ReincarnationServer()
    : next_service_id_(1)
    , running_(false)
{
}

ReincarnationServer::~ReincarnationServer() {
    stop();
}

bool ReincarnationServer::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        return false;
    }
    
    running_ = true;
    monitor_thread_ = std::thread(&ReincarnationServer::monitor_loop, this);
    
    return true;
}

void ReincarnationServer::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    
    cv_.notify_all();
    
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    // Stop all services
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, service] : services_) {
        if (service.state == ServiceState::Running && service.pid > 0) {
            // TODO: Send termination signal
            service.state = ServiceState::Stopped;
        }
    }
}

uint32_t ReincarnationServer::register_service(
    const std::string& name,
    const std::string& executable,
    const std::vector<std::string>& args,
    ServiceType type,
    const RecoveryPolicy& policy)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint32_t id = next_service_id_++;
    
    Service service;
    service.id = id;
    service.name = name;
    service.executable = executable;
    service.args = args;
    service.type = type;
    service.state = ServiceState::Stopped;
    service.policy = policy;
    service.pid = 0;
    service.restart_count = 0;
    service.last_heartbeat = std::chrono::steady_clock::now();
    
    services_[id] = std::move(service);
    
    return id;
}

bool ReincarnationServer::unregister_service(uint32_t service_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = services_.find(service_id);
    if (it == services_.end()) {
        return false;
    }
    
    // Stop the service if running
    if (it->second.state == ServiceState::Running) {
        stop_service_locked(service_id);
    }
    
    // Remove dependencies
    dependencies_.erase(service_id);
    for (auto& [id, deps] : dependencies_) {
        deps.erase(service_id);
    }
    
    services_.erase(it);
    return true;
}

bool ReincarnationServer::start_service(uint32_t service_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return start_service_locked(service_id);
}

bool ReincarnationServer::start_service_locked(uint32_t service_id) {
    auto it = services_.find(service_id);
    if (it == services_.end()) {
        return false;
    }
    
    Service& service = it->second;
    
    if (service.state == ServiceState::Running) {
        return true;  // Already running
    }
    
    // Check dependencies
    auto deps_it = dependencies_.find(service_id);
    if (deps_it != dependencies_.end()) {
        for (uint32_t dep_id : deps_it->second) {
            auto dep_it = services_.find(dep_id);
            if (dep_it == services_.end() || 
                dep_it->second.state != ServiceState::Running) {
                // Dependency not satisfied
                return false;
            }
        }
    }
    
    // TODO: Actually spawn the process using fork/exec
    // For now, simulate it
    service.pid = service_id * 1000;  // Fake PID
    service.state = ServiceState::Running;
    service.last_heartbeat = std::chrono::steady_clock::now();
    
    return true;
}

bool ReincarnationServer::stop_service(uint32_t service_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return stop_service_locked(service_id);
}

bool ReincarnationServer::stop_service_locked(uint32_t service_id) {
    auto it = services_.find(service_id);
    if (it == services_.end()) {
        return false;
    }
    
    Service& service = it->second;
    
    if (service.state != ServiceState::Running) {
        return true;  // Already stopped
    }
    
    // TODO: Send termination signal to process
    service.state = ServiceState::Stopped;
    service.pid = 0;
    
    return true;
}

bool ReincarnationServer::restart_service(uint32_t service_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    stop_service_locked(service_id);
    return start_service_locked(service_id);
}

bool ReincarnationServer::add_dependency(uint32_t service_id, uint32_t depends_on_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (services_.find(service_id) == services_.end() ||
        services_.find(depends_on_id) == services_.end()) {
        return false;
    }
    
    // Check for circular dependencies
    if (has_circular_dependency(depends_on_id, service_id)) {
        return false;
    }
    
    dependencies_[service_id].insert(depends_on_id);
    return true;
}

void ReincarnationServer::record_heartbeat(uint32_t service_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = services_.find(service_id);
    if (it != services_.end()) {
        it->second.last_heartbeat = std::chrono::steady_clock::now();
    }
}

ServiceState ReincarnationServer::get_service_state(uint32_t service_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = services_.find(service_id);
    if (it == services_.end()) {
        return ServiceState::Stopped;
    }
    
    return it->second.state;
}

ServiceStatistics ReincarnationServer::get_statistics(uint32_t service_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = services_.find(service_id);
    if (it == services_.end()) {
        return ServiceStatistics{};
    }
    
    return it->second.stats;
}

void ReincarnationServer::handle_crash(pid_t pid, int exit_status, int signal) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find service with this PID
    for (auto& [id, service] : services_) {
        if (service.pid == pid) {
            service.state = ServiceState::Crashed;
            service.stats.crash_count++;
            service.stats.last_crash_time = std::chrono::steady_clock::now();
            
            // Attempt recovery
            recover_service(id);
            break;
        }
    }
}

void ReincarnationServer::monitor_loop() {
    while (running_) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // Wait for a short interval
            cv_.wait_for(lock, std::chrono::seconds(1), [this] { return !running_; });
            
            if (!running_) break;
            
            check_health();
        }
    }
}

void ReincarnationServer::check_health() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto& [id, service] : services_) {
        if (service.state != ServiceState::Running) {
            continue;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - service.last_heartbeat).count();
        
        if (elapsed > 30) {  // 30 second timeout
            // Service appears hung
            service.state = ServiceState::Crashed;
            service.stats.crash_count++;
            service.stats.last_crash_time = now;
            
            recover_service(id);
        }
    }
}

void ReincarnationServer::recover_service(uint32_t service_id) {
    auto it = services_.find(service_id);
    if (it == services_.end()) {
        return;
    }
    
    Service& service = it->second;
    
    // Check if we should attempt recovery
    if (!service.policy.auto_restart) {
        return;
    }
    
    if (service.restart_count >= service.policy.max_retries) {
        service.state = ServiceState::Failed;
        return;
    }
    
    // Increment restart count
    service.restart_count++;
    service.stats.restart_count++;
    
    // Stop the crashed service
    stop_service_locked(service_id);
    
    // Restart after delay
    // TODO: Implement delayed restart (for now restart immediately)
    if (start_service_locked(service_id)) {
        service.stats.last_restart_time = std::chrono::steady_clock::now();
    } else {
        service.state = ServiceState::Failed;
    }
}

bool ReincarnationServer::has_circular_dependency(uint32_t from, uint32_t to) const {
    std::set<uint32_t> visited;
    return has_circular_dependency_recursive(from, to, visited);
}

bool ReincarnationServer::has_circular_dependency_recursive(
    uint32_t current,
    uint32_t target,
    std::set<uint32_t>& visited) const
{
    if (current == target) {
        return true;
    }
    
    if (visited.count(current)) {
        return false;
    }
    
    visited.insert(current);
    
    auto it = dependencies_.find(current);
    if (it != dependencies_.end()) {
        for (uint32_t dep : it->second) {
            if (has_circular_dependency_recursive(dep, target, visited)) {
                return true;
            }
        }
    }
    
    return false;
}

} // namespace xinim::servers
