/**
 * @file test_lattice_network_encrypted.cpp
 * @brief Verify encrypted message transfer between two nodes using the network driver.
 */

#include "sys/error.hpp"
#include "sys/type.hpp"
#include "../kernel/lattice_ipc.hpp"
#include "../kernel/net_driver.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstring>
#include <sys/wait.h>
#include <thread>

using namespace lattice;

/// Local and remote node identifiers used in the test
static constexpr net::node_t PARENT_NODE = 0;
static constexpr net::node_t CHILD_NODE = 1;

/// UDP ports for each node
static constexpr std::uint16_t PARENT_PORT = 12500;
static constexpr std::uint16_t CHILD_PORT = 12501;

namespace {

/// Captured raw packet from the network driver
static net::Packet g_captured{};
/// Indicates whether a packet has been captured
static std::atomic<bool> g_have_packet{false};

/**
 * @brief Network callback saving the first received packet.
 */
void packet_hook(const net::Packet &pkt) {
    if (!g_have_packet.load(std::memory_order_acquire)) {
        g_captured = pkt;
        g_have_packet.store(true, std::memory_order_release);
    }
}

} // namespace

/**
 * @brief Parent process sending a message and waiting for acknowledgement.
 */
static int parent_proc(pid_t child) {
    net::init({PARENT_NODE, PARENT_PORT});
    net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

    g_graph = Graph{};
    lattice_connect(1, 2, CHILD_NODE);

    message msg{};
    msg.m_type = 77;
    assert(lattice_send(1, 2, msg) == OK);

    message reply{};
    for (;;) {
        poll_network();
        if (lattice_recv(2, &reply) == OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(reply.m_type == 88);

    int status = 0;
    waitpid(child, &status, 0);
    net::shutdown();
    return status;
}

/**
 * @brief Child process validating encryption and responding to the parent.
 */
static int child_proc() {
    net::init({CHILD_NODE, CHILD_PORT});
    net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT);
    net::set_recv_callback(packet_hook);

    g_graph = Graph{};
    lattice_connect(2, 1, PARENT_NODE);

    // Wait for the incoming packet to be captured
    while (!g_have_packet.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Build plaintext representation of the expected packet
    std::vector<std::byte> plain(sizeof(xinim::pid_t) * 2 + sizeof(message));
    auto *ids = reinterpret_cast<xinim::pid_t *>(plain.data());
    ids[0] = 1;
    ids[1] = 2;
    message expect{};
    expect.m_type = 77;
    std::memcpy(plain.data() + sizeof(xinim::pid_t) * 2, &expect, sizeof(expect));

    // Ensure the on-the-wire payload differs from plaintext
    assert(g_captured.payload != plain);

    // Poll until the message is processed
    message incoming{};
    for (;;) {
        poll_network();
        if (lattice_recv(1, &incoming) == OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(incoming.m_type == 77);

    // Reply back to the parent
    message ack{};
    ack.m_type = 88;
    assert(lattice_send(2, 1, ack) == OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    net::shutdown();
    return 0;
}

/**
 * @brief Test harness launching the parent and child processes.
 */
int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    }
    return parent_proc(pid);
}
/**
 * @file test_lattice_network_encrypted.cpp
 * @brief Verify encrypted message transfer between two nodes using the network driver.
 */

#include "sys/error.hpp"
#include "sys/type.hpp"
#include "../kernel/lattice_ipc.hpp"
#include "../kernel/net_driver.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstring>
#include <sys/wait.h>
#include <thread>

using namespace lattice;

/// Local and remote node identifiers used in the test
static constexpr net::node_t PARENT_NODE = 0;
static constexpr net::node_t CHILD_NODE = 1;

/// UDP ports for each node
static constexpr std::uint16_t PARENT_PORT = 12500;
static constexpr std::uint16_t CHILD_PORT = 12501;

namespace {

/// Captured raw packet from the network driver
static net::Packet g_captured{};
/// Indicates whether a packet has been captured
static std::atomic<bool> g_have_packet{false};

/**
 * @brief Network callback saving the first received packet.
 */
void packet_hook(const net::Packet &pkt) {
    if (!g_have_packet.load(std::memory_order_acquire)) {
        g_captured = pkt;
        g_have_packet.store(true, std::memory_order_release);
    }
}

} // namespace

/**
 * @brief Parent process sending a message and waiting for acknowledgement.
 */
static int parent_proc(pid_t child) {
    net::driver.init({PARENT_NODE, PARENT_PORT});
    net::driver.add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

    g_graph = Graph{};
    lattice_connect(1, 2, CHILD_NODE);

    message msg{};
    msg.m_type = 77;
    assert(lattice_send(1, 2, msg) == OK);

    message reply{};
    for (;;) {
        poll_network();
        if (lattice_recv(2, &reply) == OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(reply.m_type == 88);

    int status = 0;
    waitpid(child, &status, 0);
    net::driver.shutdown();
    return status;
}

/**
 * @brief Child process validating encryption and responding to the parent.
 */
static int child_proc() {
    net::driver.init({CHILD_NODE, CHILD_PORT});
    net::driver.add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT);
    net::driver.set_recv_callback(packet_hook);

    g_graph = Graph{};
    lattice_connect(2, 1, PARENT_NODE);

    // Wait for the incoming packet to be captured
    while (!g_have_packet.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Build plaintext representation of the expected packet
    std::vector<std::byte> plain(sizeof(xinim::pid_t) * 2 + sizeof(message));
    auto *ids = reinterpret_cast<xinim::pid_t *>(plain.data());
    ids[0] = 1;
    ids[1] = 2;
    message expect{};
    expect.m_type = 77;
    std::memcpy(plain.data() + sizeof(xinim::pid_t) * 2, &expect, sizeof(expect));

    // Ensure the on-the-wire payload differs from plaintext
    assert(g_captured.payload != plain);

    // Poll until the message is processed
    message incoming{};
    for (;;) {
        poll_network();
        if (lattice_recv(1, &incoming) == OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(incoming.m_type == 77);

    // Reply back to the parent
    message ack{};
    ack.m_type = 88;
    assert(lattice_send(2, 1, ack) == OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    net::driver.shutdown();
    return 0;
}

/**
 * @brief Test harness launching the parent and child processes.
 */
int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    }
    return parent_proc(pid);
}
