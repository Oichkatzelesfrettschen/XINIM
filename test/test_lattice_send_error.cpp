/**
 * @file test_lattice_send_error.cpp
 * @brief Validate error handling when sending to an unknown node.
 */

#include "sys/error.hpp"
#include "../kernel/lattice_ipc.hpp"

#include <cassert>

using namespace lattice;

/**
 * @brief Ensure lattice_send reports EIO for unknown remote nodes.
 */
int main() {
    g_graph = Graph{};
    lattice_connect(1, 2, 99); // Connect to unregistered node

    message msg{};
    msg.m_type = 1;
    int rc = lattice_send(1, 2, msg);
    assert(rc == static_cast<int>(ErrorCode::EIO));
    return 0;
}
