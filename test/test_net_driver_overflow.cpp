/**
 * @file test_net_driver_overflow.cpp
 * @brief Unified test suite for net::OverflowPolicy::DropOldest and Overwrite.
 *        Validates queue overflow handling with enhanced features: logging, error handling,
 *        configurable tests, and cross-policy verification.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <sys/wait.h>
#include <thread>
#include <vector>
#include <string>

using namespace std::chrono_literals;

namespace {

constexpr net::node_t PARENT_NODE = 0;
constexpr net::node_t CHILD_NODE = 1;
constexpr std::uint16_t PARENT_PORT = 14100;
constexpr std::uint16_t CHILD_PORT = 14101;

/**
 * @brief TestRunner class to encapsulate and parameterize overflow tests.
 */
class TestRunner {
public:
    TestRunner(net::OverflowPolicy policy, size_t queue_size = 1)
        : policy_(policy), queue_size_(queue_size) {}

    /**
     * @brief Runs the full test for the specified policy.
     * @param child_pid PID of the child process.
     * @return Exit status from the child.
     */
    int run_test(pid_t child_pid) {
        net::driver.init(net::Config{PARENT_NODE, PARENT_PORT, queue_size_, policy_});
        net::driver.add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

        net::Packet pkt{};
        // Enhanced timeout with logging
        constexpr auto timeout = 5s;
        auto start_time = std::chrono::steady_clock::now();
        while (!net::driver.recv(pkt)) {
            if (std::chrono::steady_clock::now() - start_time > timeout) {
                std::cerr << "Timeout waiting for child readiness in " << policy_to_string() << " test." << std::endl;
                net::driver.shutdown();
                std::exit(EXIT_FAILURE);
            }
            std::this_thread::sleep_for(10ms);
        }

        std::array<std::byte, 1> start_signal{std::byte{0}};
        auto send_result = net::driver.send(CHILD_NODE, start_signal);
        if (send_result != std::errc{}) {
            std::cerr << "Failed to send start signal: " << std::make_error_code(send_result).message() << std::endl;
            net::driver.shutdown();
            return EXIT_FAILURE;
        }

        // Allow child to send packets
        std::this_thread::sleep_for(100ms);

        std::vector<std::byte> received{};
        while (net::driver.recv(pkt)) {
            if (!pkt.payload.empty()) {
                received.push_back(pkt.payload.front());
                std::cout << "Received packet: " << static_cast<int>(pkt.payload.front()) << std::endl;
            }
        }

        // Policy-specific validation
        if (policy_ == net::OverflowPolicy::DropOldest) {
            assert(received.size() == 1 && "DropOldest should retain only the newest packet");
            assert(received[0] == std::byte{2} && "DropOldest should drop oldest, keep newest");
        } else if (policy_ == net::OverflowPolicy::Overwrite) {
            assert(received.size() == 1 && "Overwrite should retain only the last packet");
            assert(received[0] == std::byte{2} && "Overwrite should keep the overwritten value");
        }

        int status = 0;
        waitpid(child_pid, &status, 0);
        net::driver.shutdown();
        return status;
    }

private:
    std::string policy_to_string() const {
        return (policy_ == net::OverflowPolicy::DropOldest) ? "DropOldest" : "Overwrite";
    }

    net::OverflowPolicy policy_;
    size_t queue_size_;
};

/**
 * @brief Child process: Sends packets to trigger overflow.
 * @param policy The overflow policy being tested.
 * @return Always zero on success.
 */
int child_proc(net::OverflowPolicy policy) {
    net::driver.init(net::Config{CHILD_NODE, CHILD_PORT});
    net::driver.add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT);

    net::Packet pkt{};
    while (!net::driver.recv(pkt)) {
        std::this_thread::sleep_for(10ms);
    }

    std::array<std::byte, 1> one{std::byte{1}};
    std::array<std::byte, 1> two{std::byte{2}};
    net::driver.send(PARENT_NODE, one);
    net::driver.send(PARENT_NODE, two);

    std::this_thread::sleep_for(50ms);
    net::driver.shutdown();
    return 0;
}

} // namespace

/**
 * @brief Entry point: Runs tests for both policies sequentially.
 */
int main() {
    std::vector<net::OverflowPolicy> policies = {net::OverflowPolicy::DropOldest, net::OverflowPolicy::Overwrite};
    for (auto policy : policies) {
        std::cout << "Running test for policy: " << (policy == net::OverflowPolicy::DropOldest ? "DropOldest" : "Overwrite") << std::endl;
        pid_t pid = fork();
        if (pid == 0) {
            return child_proc(policy);
        }
        TestRunner runner(policy);
        int result = runner.run_test(pid);
        if (result != 0) {
            std::cerr << "Test failed for policy." << std::endl;
            return result;
        }
    }
    std::cout << "All tests passed." << std::endl;
    return 0;
}
