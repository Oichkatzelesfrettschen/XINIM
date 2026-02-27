/**
 * @file test_core_types.cpp
 * @brief Compile-time and runtime verification of xinim::core_types.hpp.
 */

#include "core_types.hpp"
#include <cassert>
#include <climits>

int main() {
    // Verify type sizes
    static_assert(sizeof(xinim::phys_addr_t) == 8);
    static_assert(sizeof(xinim::virt_addr_t) == 8);
    static_assert(sizeof(xinim::pid_t) == 4);
    static_assert(sizeof(xinim::uid_t) == 4);
    static_assert(sizeof(xinim::gid_t) == 4);
    static_assert(sizeof(xinim::dev_t) == 4);
    static_assert(sizeof(xinim::ino_t) == 8);
    static_assert(sizeof(xinim::mode_t) == 4);
    static_assert(sizeof(xinim::off_t) == 8);
    static_assert(sizeof(xinim::time_t) == 8);
    static_assert(sizeof(xinim::hw::port_t) == 2);

    // Verify signedness
    static_assert(static_cast<xinim::pid_t>(-1) < 0, "pid_t must be signed");
    static_assert(static_cast<xinim::off_t>(-1) < 0, "off_t must be signed");
    static_assert(static_cast<xinim::ssize_t>(-1) < 0, "ssize_t must be signed");

    // OK constant
    static_assert(xinim::OK == 0);

    // NIL_PTR
    assert(NIL_PTR == nullptr);

    return 0;
}
