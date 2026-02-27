#include "service.hpp"
#include <cstring>

namespace svc {

ServiceManager service_manager{};

void ServiceManager::register_service(xinim::pid_t pid, std::string_view name) noexcept {
    if (service_count_ < MAX_SERVICES) {
        auto& info = services_[service_count_++];
        info.pid = pid;
        std::strncpy(info.name, name.data(), sizeof(info.name) - 1);
        info.active = true;
    }
}

bool ServiceManager::handle_crash(xinim::pid_t pid) noexcept {
    auto* svc = get_service(pid);
    if (!svc) return false;

    if (svc->restart_count < 5) {
        svc->restart_count++;
        return true; // OK to restart
    }
    
    svc->active = false;
    return false; // Reached limit
}

ServiceInfo* ServiceManager::get_service(xinim::pid_t pid) noexcept {
    for (int i = 0; i < service_count_; ++i) {
        if (services_[i].pid == pid) return &services_[i];
    }
    return nullptr;
}

} // namespace svc
