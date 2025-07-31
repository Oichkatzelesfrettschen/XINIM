/**
 * @file test_lattice_network.cpp
 * @brief Verify cross-node message delivery over UDP.
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

static constexpr net::node_t PARENT_NODE = 0;
static constexpr net::node_t CHILD_NODE = 1;
static constexpr uint16_t PARENT_PORT = 12000;
static constexpr uint16_t CHILD_PORT = 12001;

/** Parent side logic sending a message and waiting for a reply. */
static int parent_proc(pid_t child) {
    net::init({PARENT_NODE, PARENT_PORT});
    net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

    g_graph = Graph{};
    lattice_connect(1, 2, CHILD_NODE);

    message msg{};
    msg.m_type = 42;
    assert(lattice_send(1, 2, msg) == OK);

    message reply{};
    for (;;) {
        poll_network();
        if (lattice_recv(2, &reply) == OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(reply.m_type == 99);

    int status = 0;
    waitpid(child, &status, 0);
    net::shutdown();
    return status;
}

/** Child process responding to the parent's message. */
static int child_proc() {
    net::init({CHILD_NODE, CHILD_PORT});
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
    assert(incoming.m_type == 42);

    message ack{};
    ack.m_type = 99;
    assert(lattice_send(2, 1, ack) == OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    net::shutdown();
    return 0;
}

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    }
    return parent_proc(pid);
}
/**
 * @file test_lattice_network.cpp
 * @brief Verify cross-node message delivery over UDP.
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

static constexpr net::node_t PARENT_NODE = 0;
static constexpr net::node_t CHILD_NODE = 1;
static constexpr uint16_t PARENT_PORT = 12000;
static constexpr uint16_t CHILD_PORT = 12001;

/** Parent side logic sending a message and waiting for a reply. */
static int parent_proc(pid_t child) {
    net::driver.init({PARENT_NODE, PARENT_PORT});
    net::driver.add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

    g_graph = Graph{};
    lattice_connect(1, 2, CHILD_NODE);

    message msg{};
    msg.m_type = 42;
    assert(lattice_send(1, 2, msg) == OK);

    message reply{};
    for (;;) {
        poll_network();
        if (lattice_recv(2, &reply) == OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(reply.m_type == 99);

    int status = 0;
    waitpid(child, &status, 0);
    net::driver.shutdown();
    return status;
}

/** Child process responding to the parent's message. */
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
    assert(incoming.m_type == 42);

    message ack{};
    ack.m_type = 99;
    assert(lattice_send(2, 1, ack) == OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    net::driver.shutdown();
    return 0;
}

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    }
    return parent_proc(pid);
}
