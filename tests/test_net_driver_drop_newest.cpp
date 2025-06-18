/**
 * @file test_net_driver_drop_newest.cpp
 * @brief Validate queue overflow handling for net::OverflowPolicy::DropNewest.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <sys/wait.h>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

constexpr net::node_t PARENT_NODE = 0;
constexpr net::node_t CHILD_NODE = 1;
constexpr std::uint16_t PARENT_PORT = 14200;
constexpr std::uint16_t CHILD_PORT = 14201;

/**
 * @brief Parent instructs the child and verifies only the first packet remains.
 *
 * After the child sends a burst of packets, the queue is drained. The packet
 * count and contents confirm that the newest packet was dropped while the oldest
 * was preserved.
 *
 * @param child PID of the forked child process.
 * @return Exit status from the child.
 */
int parent_proc(pid_t child) {
    net::init(net::Config{PARENT_NODE, PARENT_PORT, 1, net::OverflowPolicy::DropNewest});
    net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

    net::Packet pkt{};
    constexpr auto timeout = 5s;
    auto start = std::chrono::steady_clock::now();
    while (!net::recv(pkt)) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            std::cerr << "Timeout waiting for child readiness." << std::endl;
            std::exit(EXIT_FAILURE);
        }
        std::this_thread::sleep_for(10ms);
    }

    std::array<std::byte, 1> start_pkt{std::byte{0}};
    assert(net::send(CHILD_NODE, start_pkt) == std::errc{});

    std::this_thread::sleep_for(100ms);

    std::vector<std::byte> received{};
    while (net::recv(pkt)) {
        if (!pkt.payload.empty()) {
            received.push_back(pkt.payload.front());
        }
    }

    assert(received.size() == 1);
    assert(received[0] == std::byte{1});

    int status = 0;
    waitpid(child, &status, 0);
    net::shutdown();
    return status;
}

/**
 * @brief Child sends two packets in rapid succession to overflow the parent.
 *
 * The child waits for a start signal before transmitting two one-byte packets
 * back-to-back. A short delay gives the parent time to drain the queue.
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
    assert(net::send(PARENT_NODE, one) == std::errc{});
    assert(net::send(PARENT_NODE, two) == std::errc{});

    std::this_thread::sleep_for(50ms);
    net::shutdown();
    return 0;
}

} // namespace

/**
 * @brief Entry point spawning a child to run the DropNewest overflow test.
 */
int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    }
    return parent_proc(pid);
}
