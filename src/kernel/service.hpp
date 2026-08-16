#pragma once
#include "../include/xinim/core_types.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace svc {

    /** @brief Service status and monitoring information. */
    struct ServiceInfo {
        xinim::pid_t pid{-1};
        char name[32]{};
        int restart_count{0};
        bool active{false};
    };

    /**
     * @brief Management of system services and dependencies.
     * Refactored for bare-metal: uses fixed-size array.
     */
    class ServiceManager {
    public:
        static constexpr int MAX_SERVICES = 32;

        ServiceManager() = default;

        /** @brief Register a new service. */
        void register_service(xinim::pid_t pid, std::string_view name) noexcept;

        /** @brief Handle service crash and determine if restart is allowed. */
        bool handle_crash(xinim::pid_t pid) noexcept;

        /** @brief Get service info by PID. */
        ServiceInfo *get_service(xinim::pid_t pid) noexcept;

    private:
        static constexpr std::size_t kServiceCapacity = static_cast<std::size_t>(MAX_SERVICES);
        std::array<ServiceInfo, kServiceCapacity> services_{};
        std::size_t service_count_{0};
    };

    extern ServiceManager service_manager;

} // namespace svc
