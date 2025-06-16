/**
 * @file test_net_driver_overflow.cpp
 * @brief Validate queue overflow handling for net::OverflowPolicy::Overwrite.
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
constexpr std::uint16_t PARENT_PORT = 14100;
constexpr std::uint16_t CHILD_PORT = 14101;

/**
 * @brief Parent process instructs the child and verifies the last packet is kept.
 *
 * @param child PID of the forked child process.
 * @return Exit status from the child.
 */
int parent_proc(pid_t child) {
    net::init({PARENT_NODE, PARENT_PORT, 1, net::OverflowPolicy::Overwrite});
    net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

    net::Packet pkt{};
    // Wait for the child to signal readiness
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }

    std::array<std::byte, 1> start{std::byte{0}};
    net::send(CHILD_NODE, start);

    std::this_thread::sleep_for(100ms);

    if (!net::recv(pkt)) {
        return 1; // no packet received
    }
    assert(pkt.payload.size() == 1);
    assert(pkt.payload[0] == std::byte{2});

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
    net::init({CHILD_NODE, CHILD_PORT});
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
 * @brief Entry point spawning a child to run the overflow test.
 */
int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    }
    return parent_proc(pid);
}
