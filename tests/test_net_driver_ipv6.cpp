/**
 * @file test_net_driver_ipv6.cpp
 * @brief Validate UDP and TCP packet delivery over IPv6 loopback.
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

namespace {

constexpr net::node_t PARENT_NODE = 0;
constexpr net::node_t CHILD_NODE = 1;
constexpr uint16_t UDP_PARENT_PORT = 17000;
constexpr uint16_t UDP_CHILD_PORT = 17001;
constexpr uint16_t TCP_PARENT_PORT = 17002;
constexpr uint16_t TCP_CHILD_PORT = 17003;

/**
 * @brief Child process for the UDP phase.
 */
int udp_child() {
    net::init(
        net::Config{CHILD_NODE, UDP_CHILD_PORT, 0, net::OverflowPolicy::DropNewest, NODE_ID_FILE});
    net::add_remote(PARENT_NODE, "::1", UDP_PARENT_PORT, net::Protocol::UDP);

    std::array<std::byte, 1> ready{std::byte{0}};
    assert(net::send(PARENT_NODE, ready) == std::errc{});

    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == PARENT_NODE);
    assert(pkt.payload.size() == 3);

    std::array<std::byte, 3> reply{std::byte{9}, std::byte{8}, std::byte{7}};
    assert(net::send(PARENT_NODE, reply) == std::errc{});

    std::this_thread::sleep_for(50ms);
    net::shutdown();
    return 0;
}

/**
 * @brief Parent process for the UDP phase.
 */
int udp_parent(pid_t child) {
    net::init(net::Config{PARENT_NODE, UDP_PARENT_PORT, 0, net::OverflowPolicy::DropNewest,
                          NODE_ID_FILE});
    net::add_remote(CHILD_NODE, "::1", UDP_CHILD_PORT, net::Protocol::UDP);

    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == CHILD_NODE);

    std::array<std::byte, 3> payload{std::byte{1}, std::byte{2}, std::byte{3}};
    assert(net::send(CHILD_NODE, payload) == std::errc{});

    do {
        std::this_thread::sleep_for(10ms);
    } while (!net::recv(pkt));

    assert(pkt.src_node == CHILD_NODE);
    assert(pkt.payload.size() == payload.size());

    int status = 0;
    waitpid(child, &status, 0);
    net::shutdown();
    return status;
}

/**
 * @brief Child process for the TCP phase.
 */
int tcp_child() {
    net::init(
        net::Config{CHILD_NODE, TCP_CHILD_PORT, 0, net::OverflowPolicy::DropNewest, NODE_ID_FILE});
    net::add_remote(PARENT_NODE, "::1", TCP_PARENT_PORT, net::Protocol::TCP);

    std::array<std::byte, 1> ready{std::byte{0}};
    assert(net::send(PARENT_NODE, ready) == std::errc{});

    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == PARENT_NODE);
    assert(pkt.payload.size() == 3);

    std::array<std::byte, 3> reply{std::byte{9}, std::byte{8}, std::byte{7}};
    assert(net::send(PARENT_NODE, reply) == std::errc{});

    std::this_thread::sleep_for(50ms);
    net::shutdown();
    return 0;
}

/**
 * @brief Parent process for the TCP phase.
 */
int tcp_parent(pid_t child) {
    net::init(net::Config{PARENT_NODE, TCP_PARENT_PORT, 0, net::OverflowPolicy::DropNewest,
                          NODE_ID_FILE});
    net::add_remote(CHILD_NODE, "::1", TCP_CHILD_PORT, net::Protocol::TCP);

    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == CHILD_NODE);

    std::array<std::byte, 3> payload{std::byte{1}, std::byte{2}, std::byte{3}};
    assert(net::send(CHILD_NODE, payload) == std::errc{});

    do {
        std::this_thread::sleep_for(10ms);
    } while (!net::recv(pkt));

    assert(pkt.src_node == CHILD_NODE);
    assert(pkt.payload.size() == payload.size());

    int status = 0;
    waitpid(child, &status, 0);
    net::shutdown();
    return status;
}

} // namespace

/**
 * @brief Entry point running UDP then TCP tests.
 */
int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return udp_child();
    }
    if (udp_parent(pid) != 0) {
        return 1;
    }

    pid = fork();
    if (pid == 0) {
        return tcp_child();
    }
    return tcp_parent(pid);
}
