/**
 * @file test_net_driver_concurrency.cpp
 * @brief Exercise concurrent add_remote and send operations.
 */

#include "../kernel/net_driver.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
    constexpr net::node_t SELF = 50;
    constexpr uint16_t PORT = 16550;
    constexpr int THREADS = 4;

    net::init(net::Config{SELF, PORT});

    std::atomic<int> received{0};
    net::set_recv_callback([&](const net::Packet &) { received.fetch_add(1); });

    auto worker = [&](int idx) {
        net::node_t node = static_cast<net::node_t>(idx + 1);
        net::add_remote(node, "127.0.0.1", PORT);
        std::array<std::byte, 1> payload{std::byte{static_cast<unsigned char>(idx)}};
        assert(net::send(node, payload) == std::errc{});
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS; ++i) {
        threads.emplace_back(worker, i);
    }
    for (auto &t : threads) {
        t.join();
    }

    auto start = std::chrono::steady_clock::now();
    while (received.load() < THREADS && std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }

    assert(received.load() == THREADS);

    net::shutdown();
    return 0;
}
/**
 * @file test_net_driver_concurrency.cpp
 * @brief Exercise concurrent add_remote and send operations.
 */

#include "../kernel/net_driver.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
    constexpr net::node_t SELF = 50;
    constexpr uint16_t PORT = 16550;
    constexpr int THREADS = 4;

    net::driver.init(net::Config{SELF, PORT});

    std::atomic<int> received{0};
    net::driver.set_recv_callback([&](const net::Packet &) { received.fetch_add(1); });

    auto worker = [&](int idx) {
        net::node_t node = static_cast<net::node_t>(idx + 1);
        net::driver.add_remote(node, "127.0.0.1", PORT);
        std::array<std::byte, 1> payload{std::byte{static_cast<unsigned char>(idx)}};
        assert(net::driver.send(node, payload) == std::errc{});
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS; ++i) {
        threads.emplace_back(worker, i);
    }
    for (auto &t : threads) {
        t.join();
    }

    auto start = std::chrono::steady_clock::now();
    while (received.load() < THREADS && std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }

    assert(received.load() == THREADS);

    net::driver.shutdown();
    return 0;
}
