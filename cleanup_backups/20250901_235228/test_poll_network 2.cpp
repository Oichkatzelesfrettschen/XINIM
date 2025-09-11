/**
 * @file test_poll_network.cpp
 * @brief Verify that poll_network delivers messages between two nodes.
 *
 * The test performs the following setup steps for each node:
 * 1. Call net::init with a unique UDP port.
 * 2. Register the peer with net::add_remote.
 * 3. Establish a lattice IPC channel via lattice_connect.
 * 4. Send a message from the parent to the child.
 * 5. Repeatedly call poll_network() and lattice_recv() until the message is
 *    received.
 *
 * The child replies with its net::local_node() identifier and the parent ensures
 * the two nodes report different IDs.
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
static constexpr std::uint16_t PARENT_PORT = 15000;
/// UDP port used by the child
static constexpr std::uint16_t CHILD_PORT = 15001;

/**
 * @brief Child process logic responding with its node identifier.
 *
 * The child waits for a handshake from the parent, then replies with its
 * net::local_node() value encoded in the message type.
 *
 * @return Process exit code used by the parent.
 */
static int child_proc() {
    net::init(net::Config{CHILD_NODE, CHILD_PORT});
    net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT);

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
    assert(incoming.m_type == 0xCAFE);

    message reply{};
    reply.m_type = net::local_node();
    lattice_send(2, 1, reply);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    net::shutdown();
    return 0;
}

/**
 * @brief Parent process sending the handshake and validating the reply.
 *
 * @param child Process ID of the forked child.
 * @return Status code returned from waitpid.
 */
static int parent_proc(pid_t child) {
    net::init(net::Config{PARENT_NODE, PARENT_PORT});
    net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

    g_graph = Graph{};
    lattice_connect(1, 2, CHILD_NODE);

    message msg{};
    msg.m_type = 0xCAFE;
    lattice_send(1, 2, msg);

    message reply{};
    for (;;) {
        poll_network();
        if (lattice_recv(2, &reply) == OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto parent_id = net::local_node();
    auto child_id = reply.m_type;
    assert(parent_id != child_id);

    int status = 0;
    waitpid(child, &status, 0);
    net::shutdown();
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
/**
 * @file test_poll_network.cpp
 * @brief Verify that poll_network delivers messages between two nodes.
 *
 * The test performs the following setup steps for each node:
 * 1. Call net::init with a unique UDP port.
 * 2. Register the peer with net::driver.add_remote.
 * 3. Establish a lattice IPC channel via lattice_connect.
 * 4. Send a message from the parent to the child.
 * 5. Repeatedly call poll_network() and lattice_recv() until the message is
 *    received.
 *
 * The child replies with its net::driver.local_node() identifier and the parent ensures
 * the two nodes report different IDs.
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
static constexpr std::uint16_t PARENT_PORT = 15000;
/// UDP port used by the child
static constexpr std::uint16_t CHILD_PORT = 15001;

/**
 * @brief Child process logic responding with its node identifier.
 *
 * The child waits for a handshake from the parent, then replies with its
 * net::driver.local_node() value encoded in the message type.
 *
 * @return Process exit code used by the parent.
 */
static int child_proc() {
    net::driver.init(net::Config{CHILD_NODE, CHILD_PORT});
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
    assert(incoming.m_type == 0xCAFE);

    message reply{};
    reply.m_type = net::driver.local_node();
    lattice_send(2, 1, reply);

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
    net::driver.init(net::Config{PARENT_NODE, PARENT_PORT});
    net::driver.add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

    g_graph = Graph{};
    lattice_connect(1, 2, CHILD_NODE);

    message msg{};
    msg.m_type = 0xCAFE;
    lattice_send(1, 2, msg);

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
