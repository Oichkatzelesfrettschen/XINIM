/**
 * @file test_net_driver_tcp.cpp
 * @brief Validate TCP packet delivery between two nodes.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#include <chrono>
#include <sys/wait.h>
#include <thread>

using namespace std::chrono_literals;

namespace {

constexpr net::node_t PARENT_NODE = 0;
constexpr net::node_t CHILD_NODE = 1;
constexpr std::uint16_t PARENT_PORT = 15000;
constexpr std::uint16_t CHILD_PORT = 15001;

int parent_proc(pid_t child) {
    net::init({PARENT_NODE, PARENT_PORT});
    net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT, net::Protocol::TCP);
    assert(net::local_node() != 0);

    std::this_thread::sleep_for(100ms);

    net::Packet pkt{};
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }

    std::array<std::byte, 3> data{std::byte{1}, std::byte{2}, std::byte{3}};
    net::send(CHILD_NODE, data);

    do {
        std::this_thread::sleep_for(10ms);
    } while (!net::recv(pkt));

    assert(pkt.src_node == CHILD_NODE);
    assert(pkt.payload.size() == 3);
    assert(pkt.payload[0] == std::byte{9});

    int status = 0;
    waitpid(child, &status, 0);
    net::shutdown();
    return status;
}

int child_proc() {
    net::init({CHILD_NODE, CHILD_PORT});
    net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT, net::Protocol::TCP);

    std::array<std::byte, 1> ready{std::byte{0}};
    net::send(PARENT_NODE, ready);

    net::Packet pkt{};
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == PARENT_NODE);

    std::array<std::byte, 3> reply{std::byte{9}, std::byte{8}, std::byte{7}};
    net::send(PARENT_NODE, reply);

    std::this_thread::sleep_for(50ms);
    net::shutdown();
    return 0;
}

} // namespace

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    }
    return parent_proc(pid);
}
