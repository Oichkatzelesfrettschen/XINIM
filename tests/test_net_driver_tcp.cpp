/**
 * @file test_net_driver_tcp.cpp
 * @brief Validate TCP packet delivery between two nodes.
 */

#include "../kernel/net_driver.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

namespace {

constexpr net::node_t PARENT_NODE = 0;
constexpr net::node_t CHILD_NODE = 1;
constexpr uint16_t PARENT_PORT = 15000;
constexpr uint16_t CHILD_PORT = 15001;

/** Parent process: receives a “ready” packet, sends payload, and checks reply. */
int parent_proc(pid_t child_pid) {
    net::init(net::Config{PARENT_NODE, PARENT_PORT});
    net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT, net::Protocol::TCP);

    // Wait for child to signal readiness
    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == CHILD_NODE);

    // Send a 3-byte payload
    std::array<std::byte, 3> data{std::byte{1}, std::byte{2}, std::byte{3}};
    assert(net::send(CHILD_NODE, data) == std::errc{});

    // Wait for reply
    do {
        std::this_thread::sleep_for(10ms);
    } while (!net::recv(pkt));

    assert(pkt.src_node == CHILD_NODE);
    assert(pkt.payload.size() == data.size());
    assert(pkt.payload[0] == std::byte{9});
    assert(pkt.payload[1] == std::byte{8});
    assert(pkt.payload[2] == std::byte{7});

    // Clean up
    int status = 0;
    waitpid(child_pid, &status, 0);
    net::shutdown();
    return status;
}

/** Child process: signals ready, echoes back a 3-byte reply. */
int child_proc() {
    net::init(net::Config{CHILD_NODE, CHILD_PORT});
    net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT, net::Protocol::TCP);

    // Signal readiness
    std::array<std::byte, 1> ready{std::byte{0}};
    assert(net::send(PARENT_NODE, ready) == std::errc{});

    // Wait for parent payload
    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == PARENT_NODE);
    assert(pkt.payload.size() == 3);

    // Send back reply [9,8,7]
    std::array<std::byte, 3> reply{std::byte{9}, std::byte{8}, std::byte{7}};
    assert(net::send(PARENT_NODE, reply) == std::errc{});

    // Allow time for delivery before shutdown
    std::this_thread::sleep_for(50ms);
    net::shutdown();
    return 0;
}

} // namespace

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    } else if (pid > 0) {
        return parent_proc(pid);
    } else {
        // fork failed
        return 1;
    }
}
