#include "service.hpp"

#include <cstring>

namespace svc {

    ServiceManager service_manager{};

    void ServiceManager::register_service(xinim::pid_t pid, std::string_view name) noexcept {
        if (service_count_ < services_.size()) {
            auto &info = services_[service_count_++];
            info.pid = pid;
            const std::size_t copy_length =
                name.size() < (sizeof(info.name) - 1U) ? name.size() : (sizeof(info.name) - 1U);
            if (copy_length != 0U) {
                std::memcpy(info.name, name.data(), copy_length);
            }
            info.name[copy_length] = '\0';
            info.active = true;
        }
    }

    bool ServiceManager::handle_crash(xinim::pid_t pid) noexcept {
        auto *svc = get_service(pid);
        if (!svc)
            return false;

        if (svc->restart_count < 5) {
            svc->restart_count++;
            return true; // OK to restart
        }

        svc->active = false;
        return false; // Reached limit
    }

    ServiceInfo *ServiceManager::get_service(xinim::pid_t pid) noexcept {
        for (std::size_t service_index = 0; service_index < service_count_; ++service_index) {
            if (services_[service_index].pid == pid)
                return &services_[service_index];
        }
        return nullptr;
    }

} // namespace svc
