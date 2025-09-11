#define CATCH_CONFIG_MAIN
#include <array>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <csignal>
#include <ranges>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

/// \file test1.cpp
/// \brief Unit tests verifying process forking and signal handling.
///
/// These tests exercise POSIX process control primitives to ensure the
/// kernel interface behaves as expected.

/// \brief Signal counter incremented by the handler.
static std::atomic_int g_signal_count{0};

/// \brief Simple signal handler that increments \ref g_signal_count.
/// \param sig Identifier of the received signal.
void signal_handler(int sig) {
    static_cast<void>(sig);
    ++g_signal_count;
}

/// \test Verify that `fork` correctly spawns independent processes.
TEST_CASE("fork spawns children", "[process]") {
    constexpr int children = 4;
    std::array<pid_t, children> pids{};
    for (auto i : std::views::iota(0, children)) {
        const pid_t pid = ::fork();
        REQUIRE(pid >= 0);
        if (pid == 0) {
            ::_exit(i);
        }
        pids[i] = pid;
    }
    for (auto i : std::views::iota(0, children)) {
        int status{};
        REQUIRE(::waitpid(pids[i], &status, 0) == pids[i]);
        CHECK(::WIFEXITED(status));
        CHECK(::WEXITSTATUS(status) == i);
    }
}

/// \test Ensure signals are delivered to child processes.
TEST_CASE("signal handler runs in child", "[signal]") {
    g_signal_count = 0;
    const pid_t pid = ::fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        std::signal(SIGUSR1, signal_handler);
        while (g_signal_count == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        ::_exit(g_signal_count.load());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
    ::kill(pid, SIGUSR1);

    int status{};
    REQUIRE(::waitpid(pid, &status, 0) == pid);
    CHECK(::WIFEXITED(status));
    CHECK(::WEXITSTATUS(status) == 1);
}
