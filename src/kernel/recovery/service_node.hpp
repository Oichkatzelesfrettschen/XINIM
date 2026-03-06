/**
 * @file service_node.hpp
 * @brief Service descriptor for the Resurrection Server (RS).
 *
 * Each ServiceNode describes a microkernel server (VFS, PM, MM, etc.)
 * with its dependencies, restart policy, and current state.
 * Inspired by Minix 3.4.0 servers/rs/service_t.
 *
 * Freestanding -- no STL.
 */
#pragma once

#include <cstdint>
#include <cstddef>

namespace xinim::kernel::recovery {

enum class ServiceState : uint8_t {
    STOPPED   = 0,
    STARTING  = 1,
    RUNNING   = 2,
    CRASHED   = 3,
    RESTARTING = 4,
};

enum class RestartPolicy : uint8_t {
    RESTART     = 0,  // Restart the crashed service
    KILL_DEPS   = 1,  // Kill dependents, then restart all
    PANIC       = 2,  // Kernel panic -- critical service
    IGNORE      = 3,  // No action (non-essential)
};

static constexpr size_t MAX_SERVICE_NAME = 32;
static constexpr size_t MAX_DEPS = 8;

struct ServiceNode {
    char         name[MAX_SERVICE_NAME];
    int32_t      pid;
    ServiceState state;
    RestartPolicy policy;
    uint8_t      restart_count;
    uint8_t      max_restarts;   // 0 = unlimited

    // Dependency tracking: indices into RecoveryDag's service array
    int16_t      deps[MAX_DEPS];
    uint8_t      ndeps;

    // Topological sort working data
    int16_t      in_degree;      // Number of unsatisfied dependencies

    ServiceNode()
        : name{}, pid{-1}, state{ServiceState::STOPPED}
        , policy{RestartPolicy::RESTART}, restart_count{0}, max_restarts{5}
        , deps{}, ndeps{0}, in_degree{0} {}

    void set_name(const char* n) {
        size_t i = 0;
        while (i < MAX_SERVICE_NAME - 1 && n[i] != '\0') {
            name[i] = n[i];
            i++;
        }
        name[i] = '\0';
    }

    bool name_matches(const char* n) const {
        for (size_t i = 0; i < MAX_SERVICE_NAME; i++) {
            if (name[i] != n[i]) return false;
            if (name[i] == '\0') return true;
        }
        return true;
    }
};

} // namespace xinim::kernel::recovery
