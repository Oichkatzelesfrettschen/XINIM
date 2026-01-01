/**
 * @file chaos_resilience.cpp
 * @brief Chaos engineering inspired test exercising recovery semantics.
 *
 * The scenario models a stream processor that experiences random pauses and
 * injected exceptions. The harness uses deterministic seeds to keep runs
 * reproducible while still exploring a wide space of partial failures. The
 * checks focus on convergence: even after faults the ledger must reconcile to
 * the same logical commit count as a fault-free execution.
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

namespace xinim::chaos {

/**
 * @brief Resilient ledger accumulating events with replay-friendly semantics.
 */
class ChaosResilientLedger {
  public:
    ChaosResilientLedger() = default;

    /**
     * @brief Records an event, optionally injecting a synthetic failure.
     */
    void record(const std::uint64_t value, const bool inject_fault) {
        std::scoped_lock guard{mutex_};
        pending_.push_back(value);
        if (inject_fault) {
            ++faults_;
            throw std::runtime_error{"synthetic fault"};
        }
        commit_pending();
    }

    /**
     * @brief Attempts to commit any pending events.
     */
    void commit_pending() {
        for (const auto value : pending_) {
            committed_total_ += value;
            ++committed_events_;
        }
        pending_.clear();
    }

    /**
     * @brief Reconciles state after a fault by retrying pending events.
     */
    void reconcile() {
        std::scoped_lock guard{mutex_};
        commit_pending();
    }

    /**
     * @brief Returns the number of faults injected so far.
     */
    [[nodiscard]] auto fault_count() const noexcept -> std::uint64_t { return faults_.load(); }

    /**
     * @brief Returns the total committed events.
     */
    [[nodiscard]] auto committed_events() const noexcept -> std::uint64_t {
        return committed_events_.load();
    }

    /**
     * @brief Returns the sum of committed payloads.
     */
    [[nodiscard]] auto committed_total() const noexcept -> std::uint64_t {
        return committed_total_.load();
    }

  private:
    std::vector<std::uint64_t> pending_{};
    std::atomic<std::uint64_t> committed_total_{0U};
    std::atomic<std::uint64_t> committed_events_{0U};
    std::atomic<std::uint64_t> faults_{0U};
    std::mutex mutex_{};
};

} // namespace xinim::chaos

using namespace xinim::chaos;

TEST_CASE("chaos harness reconciles after injected faults", "[chaos][recovery]") {
    ChaosResilientLedger ledger{};
    std::vector<std::jthread> workers{};

    constexpr std::uint64_t events_per_worker = 32U;
    constexpr std::uint64_t worker_count = 4U;
    std::atomic<std::uint64_t> attempted{0U};

    for (std::uint64_t worker_id = 0; worker_id < worker_count; ++worker_id) {
        workers.emplace_back([worker_id, &ledger, &attempted]() {
            std::mt19937_64 rng{worker_id};
            std::bernoulli_distribution fault_gate{0.25};

            for (std::uint64_t i = 0; i < events_per_worker; ++i) {
                const auto inject_fault = fault_gate(rng);
                try {
                    ledger.record(i + 1U, inject_fault);
                    attempted.fetch_add(1U);
                } catch (const std::runtime_error &) {
                    ledger.reconcile();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{2});
            }
        });
    }

    for (auto &worker : workers) {
        worker.join();
    }

    ledger.reconcile();

    REQUIRE(ledger.committed_events() == attempted.load());
    REQUIRE(ledger.committed_total() >= attempted.load());
    REQUIRE(ledger.fault_count() > 0U);
}
