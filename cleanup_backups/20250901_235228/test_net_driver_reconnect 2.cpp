/**
 * @file test_net_driver_reconnect.cpp
 * @brief Validate automatic TCP reconnection after peer loss.
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
constexpr uint16_t PARENT_PORT = 15500;
constexpr uint16_t CHILD_PORT = 15501;

/**
 * @brief First child sends a ready byte then waits for a payload before
 *        shutting down to drop the connection.
 */
int child_once() {
    net::init(net::Config{CHILD_NODE, CHILD_PORT});
    net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT, net::Protocol::TCP);

    std::array<std::byte, 1> ready{std::byte{0}};
    assert(net::send(PARENT_NODE, ready) == std::errc{});

    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == PARENT_NODE);
    assert(pkt.payload.size() == 3);

    net::shutdown();
    return 0;
}

/** Child process: wait for payload after reconnect and acknowledge. */
int child_second() {
    net::init(net::Config{CHILD_NODE, CHILD_PORT});
    net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT, net::Protocol::TCP);

    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == PARENT_NODE);
    assert(pkt.payload.size() == 3);

    std::array<std::byte, 1> ack{std::byte{1}};
    assert(net::send(PARENT_NODE, ack) == std::errc{});
    std::this_thread::sleep_for(50ms);
    net::shutdown();
    return 0;
}

/** Parent routine orchestrating reconnection. */
int parent_proc() {
    net::init(net::Config{PARENT_NODE, PARENT_PORT});
    pid_t first_child = fork();
    if (first_child == 0) {
        return child_once();
    }

    for (int attempt = 0;; ++attempt) {
        try {
            net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT, net::Protocol::TCP);
            break;
        } catch (const std::system_error &) {
            if (attempt > 50) {
                throw;
            }
            std::this_thread::sleep_for(10ms);
        }
    }

    net::Packet pkt;
    while (!net::recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == CHILD_NODE);

    std::array<std::byte, 3> initial{std::byte{1}, std::byte{2}, std::byte{3}};
    assert(net::send(CHILD_NODE, initial) == std::errc{});

    int status = 0;
    waitpid(first_child, &status, 0);
    assert(status == 0);

    pid_t second_child = fork();
    if (second_child == 0) {
        return child_second();
    }

    std::array<std::byte, 3> payload{std::byte{1}, std::byte{2}, std::byte{3}};
    assert(net::send(CHILD_NODE, payload) == std::errc{});

    do {
        std::this_thread::sleep_for(10ms);
    } while (!net::recv(pkt));
    assert(pkt.src_node == CHILD_NODE);

    waitpid(second_child, &status, 0);
    net::shutdown();
    return status;
}

} // namespace

int main() { return parent_proc(); }
/**
 * @file test_net_driver_reconnect.cpp
 * @brief Validate automatic TCP reconnection after peer loss.
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
constexpr uint16_t PARENT_PORT = 15500;
constexpr uint16_t CHILD_PORT = 15501;

/** Child process: signal ready then exit to drop the connection. */
int child_once() {
    net::driver.init(net::Config{CHILD_NODE, CHILD_PORT});
    net::driver.add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT, net::Protocol::TCP);

    std::array<std::byte, 1> ready{std::byte{0}};
    assert(net::driver.send(PARENT_NODE, ready) == std::errc{});
    std::this_thread::sleep_for(50ms);
    net::driver.shutdown();
    return 0;
}

/** Child process: wait for payload after reconnect and acknowledge. */
int child_second() {
    net::driver.init(net::Config{CHILD_NODE, CHILD_PORT});
    net::driver.add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT, net::Protocol::TCP);

    net::Packet pkt;
    while (!net::driver.recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == PARENT_NODE);
    assert(pkt.payload.size() == 3);

    std::array<std::byte, 1> ack{std::byte{1}};
    assert(net::driver.send(PARENT_NODE, ack) == std::errc{});
    std::this_thread::sleep_for(50ms);
    net::driver.shutdown();
    return 0;
}

/** Parent routine orchestrating reconnection. */
int parent_proc() {
    net::driver.init(net::Config{PARENT_NODE, PARENT_PORT});
    pid_t first_child = fork();
    if (first_child == 0) {
        return child_once();
    }

    for (int attempt = 0;; ++attempt) {
        try {
            net::driver.add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT, net::Protocol::TCP);
            break;
        } catch (const std::system_error &) {
            if (attempt > 50) {
                throw;
            }
            std::this_thread::sleep_for(10ms);
        }
    }

    net::Packet pkt;
    while (!net::driver.recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }
    assert(pkt.src_node == CHILD_NODE);
    int status = 0;
    waitpid(first_child, &status, 0);
    assert(status == 0);

    pid_t second_child = fork();
    if (second_child == 0) {
        return child_second();
    }

    std::array<std::byte, 3> payload{std::byte{1}, std::byte{2}, std::byte{3}};
    assert(net::driver.send(CHILD_NODE, payload) == std::errc{});

    do {
        std::this_thread::sleep_for(10ms);
    } while (!net::driver.recv(pkt));
    assert(pkt.src_node == CHILD_NODE);

    waitpid(second_child, &status, 0);
    net::driver.shutdown();
    return status;
}

} // namespace

int main() { return parent_proc(); }
