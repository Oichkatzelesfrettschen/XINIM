/**
 * @file test_net_two_node.cpp
 * @brief Validate bidirectional communication between two nodes using
 *        separate net::init configurations.
 *
 * Each node binds to a distinct UDP port. The parent sends a handshake
 * message to the child and receives the child's net::driver.local_node()
 * identifier in response. The test ensures the two processes report
 * different node IDs and demonstrates the minimal setup steps:
 * 1. Initialize networking with net::init providing node ID and port.
 * 2. Register the peer with net::driver.add_remote.
 * 3. Establish a channel via lattice_connect.
 * 4. Exchange messages while polling the network with poll_network().
 */

#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../kernel/lattice_ipc.hpp"
#include "../kernel/net_driver.hpp"

#include <cassert>
#include <chrono>
#include <csignal>
#include <sys/wait.h>
#include <thread>

using namespace lattice;

/// Local node identifier for the parent process
static constexpr net::node_t PARENT_NODE = 0;
/// Local node identifier for the child process
static constexpr net::node_t CHILD_NODE = 1;
/// UDP port used by the parent
static constexpr std::uint16_t PARENT_PORT = 13000;
/// UDP port used by the child
static constexpr std::uint16_t CHILD_PORT = 13001;

/**
 * @brief Child process logic responding with its node identifier.
 *
 * The child waits for a handshake from the parent, then replies with
 * its net::driver.local_node() value encoded in the message type.
 *
 * @return Process exit code used by the parent.
 */
static int child_proc() {
    net::driver.init({CHILD_NODE, CHILD_PORT});
    net::driver.add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT);

    g_graph = Graph{};
    lattice_connect(2, 1, PARENT_NODE);

    message incoming{};
    for (;;) {
        poll_network();
        if (lattice_recv(1, &incoming) == OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(incoming.m_type == 0x1234);

    message reply{};
    reply.m_type = net::driver.local_node();
    assert(lattice_send(2, 1, reply) == OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    net::driver.shutdown();
    return 0;
}

/**
 * @brief Parent process sending the handshake and validating the reply.
 *
 * @param child Process ID of the forked child.
 * @return Status code returned from waitpid.
 */
static int parent_proc(pid_t child) {
    net::driver.init({PARENT_NODE, PARENT_PORT});
    net::driver.add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

    g_graph = Graph{};
    lattice_connect(1, 2, CHILD_NODE);

    message msg{};
    msg.m_type = 0x1234;
    assert(lattice_send(1, 2, msg) == OK);

    message reply{};
    for (;;) {
        poll_network();
        if (lattice_recv(2, &reply) == OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto parent_id = net::driver.local_node();
    auto child_id = reply.m_type;
    assert(parent_id != child_id);

    int status = 0;
    waitpid(child, &status, 0);
    net::driver.shutdown();
    return status;
}

/**
 * @brief Test entry point launching child and parent processes.
 */
int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    }
    return parent_proc(pid);
}
