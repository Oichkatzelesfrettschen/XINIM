/**
 * @file test_lattice_ipv6.cpp
 * @brief Verify encrypted lattice IPC over IPv6 including queue semantics.
 */

#include "../h/error.hpp"
#include "../h/type.hpp"
#include "../kernel/lattice_ipc.hpp"
#include "../kernel/net_driver.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <sys/wait.h>
#include <thread>

using namespace lattice;

/// Identifier for the parent node.
static constexpr net::node_t PARENT_NODE = 0;
/// Identifier for the child node.
static constexpr net::node_t CHILD_NODE = 1;
/// UDP port bound by the parent.
static constexpr std::uint16_t PARENT_PORT = 12600;
/// UDP port bound by the child.
static constexpr std::uint16_t CHILD_PORT = 12601;

namespace {

/// Captured packet from the network driver.
static net::Packet g_captured{};
/// Flag indicating a packet has been captured.
static std::atomic<bool> g_have_packet{false};

/**
 * @brief Callback invoked when a packet arrives.
 *
 * The first packet is stored to validate encryption.
 */
void packet_hook(const net::Packet &pkt) {
    if (!g_have_packet.load(std::memory_order_acquire)) {
        g_captured = pkt;
        g_have_packet.store(true, std::memory_order_release);
    }
}

} // namespace

/**
 * @brief Parent process sending a message and awaiting the reply.
 *
 * @param child PID of the forked child process.
 * @return Status code from the child.
 */
static int parent_proc(pid_t child) {
    net::init(net::Config{PARENT_NODE, PARENT_PORT});
    net::add_remote(CHILD_NODE, "::1", CHILD_PORT);

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

/**
 * @brief Child process validating encryption and queue logic.
 *
 * @return Exit status code.
 */
static int child_proc() {
    net::init(net::Config{CHILD_NODE, CHILD_PORT});
    net::add_remote(PARENT_NODE, "::1", PARENT_PORT);
    net::set_recv_callback(packet_hook);

    g_graph = Graph{};
    lattice_connect(2, 1, PARENT_NODE);

    // Wait for the packet to be captured.
    while (!g_have_packet.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Build a plaintext representation to compare against.
    std::vector<std::byte> plain(sizeof(xinim::pid_t) * 2 + sizeof(message));
    auto *ids = reinterpret_cast<xinim::pid_t *>(plain.data());
    ids[0] = 1;
    ids[1] = 2;
    message expect{};
    expect.m_type = 42;
    std::memcpy(plain.data() + sizeof(xinim::pid_t) * 2, &expect, sizeof(expect));

    // Captured payload must differ proving encryption occurred.
    assert(g_captured.payload != plain);

    // Poll until the channel queue holds the message.
    Channel *ch = nullptr;
    for (;;) {
        poll_network();
        ch = g_graph.find(1, 2, PARENT_NODE);
        if (ch && !ch->queue.empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(ch && ch->queue.size() == 1);

    // Receive and validate the message.
    message incoming{};
    assert(lattice_recv(2, &incoming) == OK);
    assert(incoming.m_type == 42);
    assert(ch->queue.empty());

    // Reply back to the parent.
    message ack{};
    ack.m_type = 99;
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
