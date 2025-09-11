/**
 * @file test_net_driver_drop.cpp
 * @brief Validate queue overflow handling for net::OverflowPolicy::Drop.
 */

#include "../kernel/net_driver.hpp"
#include <iostream>

#include <cassert>
#include <chrono>
#include <sys/wait.h>
#include <thread>

using namespace std::chrono_literals;

namespace {

constexpr net::node_t PARENT_NODE = 0;
constexpr net::node_t CHILD_NODE = 1;
constexpr std::uint16_t PARENT_PORT = 14100;
constexpr std::uint16_t CHILD_PORT = 14101;

/**
 * @brief Parent process instructs the child and verifies the first packet is kept.
 *
 * @param child PID of the forked child process.
 * @return Exit status from the child.
 */
int parent_proc(pid_t child) {
    net::init(net::Config{PARENT_NODE, PARENT_PORT, 1, net::OverflowPolicy::Drop});
    net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

    net::Packet pkt{};
    // Wait for the child to signal readiness, with timeout to avoid infinite wait
    constexpr auto timeout = 5s;
    auto begin = std::chrono::steady_clock::now();
    while (!net::recv(pkt)) {
        if (std::chrono::steady_clock::now() - begin > timeout) {
            std::cerr << "Timeout waiting for child to signal readiness." << std::endl;
            std::exit(EXIT_FAILURE);
        }
        std::this_thread::sleep_for(10ms);
    }

    std::array<std::byte, 1> start_msg{std::byte{0}};
    net::send(CHILD_NODE, start_msg);

    std::this_thread::sleep_for(100ms);

    if (!net::recv(pkt)) {
        return 1; // no packet received
    }
    assert(pkt.payload.size() == 1);
    assert(pkt.payload[0] == std::byte{1});

    int status = 0;
    waitpid(child, &status, 0);
    net::shutdown();
    return status;
}

/**
 * @brief Child sends two packets quickly to overflow the parent's queue.
 *
 * @return Always zero on success.
 */
int child_proc() {
    net::init(net::Config{CHILD_NODE, CHILD_PORT});
    net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT);

    net::Packet pkt{};
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }

    std::array<std::byte, 1> one{std::byte{1}};
    std::array<std::byte, 1> two{std::byte{2}};
    net::send(PARENT_NODE, one);
    net::send(PARENT_NODE, two);

    std::this_thread::sleep_for(50ms);
    net::shutdown();
    return 0;
}

} // namespace

/**
 * @brief Entry point spawning a child to run the drop test.
 */
int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    }
    return parent_proc(pid);
}
