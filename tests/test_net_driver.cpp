/**
 * @file test_net_driver.cpp
 * @brief Validate UDP packet delivery between two nodes.
 */

#include "../kernel/net_driver.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

/// Path for the persistent node identifier during tests
static constexpr char NODE_ID_FILE[] = "/tmp/xinim_node_id";

static constexpr net::node_t PARENT_NODE = 0;
static constexpr net::node_t CHILD_NODE = 1;
static constexpr uint16_t PARENT_PORT = 14000;
static constexpr uint16_t CHILD_PORT = 14001;

/** Parent process: verifies unknown‐peer send fails, then exchanges payloads. */
int parent_proc(pid_t child_pid) {
    // Initialize UDP driver for parent
    net::init(
        net::Config{PARENT_NODE, PARENT_PORT, 0, net::OverflowPolicy::DropNewest, NODE_ID_FILE});

    // Unknown destination should be rejected
    std::array<std::byte, 1> bogus{std::byte{0}};
    assert(net::send(99, bogus) == std::errc::host_unreachable);

    // Register child as UDP peer
    net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT, net::Protocol::UDP);
    assert(net::local_node() != 0);

    // Wait for child's readiness signal
    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == CHILD_NODE);

    // Send a 3-byte message
    std::array<std::byte, 3> data{std::byte{1}, std::byte{2}, std::byte{3}};
    assert(net::send(CHILD_NODE, data) == std::errc{});

    // Await and verify child's reply
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == CHILD_NODE);
    assert(pkt.payload.size() == data.size());
    assert(pkt.payload[0] == std::byte{9});
    assert(pkt.payload[1] == std::byte{8});
    assert(pkt.payload[2] == std::byte{7});

    // Clean up
    waitpid(child_pid, nullptr, 0);
    net::shutdown();
    return 0;
}

/** Child process: signals readiness, echoes back a 3-byte reply. */
int child_proc() {
    // Initialize UDP driver for child
    net::init(
        net::Config{CHILD_NODE, CHILD_PORT, 0, net::OverflowPolicy::DropNewest, NODE_ID_FILE});

    // Unknown destination should be rejected
    std::array<std::byte, 1> bogus{std::byte{0}};
    assert(net::send(77, bogus) == std::errc::host_unreachable);

    // Register parent as UDP peer
    net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT, net::Protocol::UDP);

    // Signal readiness to parent
    std::array<std::byte, 1> ready{std::byte{0}};
    assert(net::send(PARENT_NODE, ready) == std::errc{});

    // Receive parent's message
    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == PARENT_NODE);
    assert(pkt.payload.size() == 3);

    // Send back reply [9,8,7]
    std::array<std::byte, 3> reply{std::byte{9}, std::byte{8}, std::byte{7}};
    assert(net::send(PARENT_NODE, reply) == std::errc{});

    // Give parent time to receive
    std::this_thread::sleep_for(50ms);
    net::shutdown();
    return 0;
}

int main() {
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        return child_proc();
    } else {
        return parent_proc(pid);
    }
}
