/**
 * @file lock_benchmark.cpp
 * @brief Comprehensive performance benchmark suite for XINIM lock framework.
 *
 * Benchmarks all 5 lock types (Ticket, MCS, Adaptive, PhaseRW, Capability)
 * across multiple scenarios with varying contention levels.
 */

#include "../../src/kernel/ticket_spinlock.hpp"
#include "../../src/kernel/mcs_spinlock.hpp"
#include "../../src/kernel/adaptive_mutex.hpp"
#include "../../src/kernel/phase_rwlock.hpp"
#include "../../src/kernel/capability_mutex.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <iomanip>

using namespace xinim;
using namespace std::chrono;

// ============================================================================
// Benchmark Configuration
// ============================================================================

struct BenchConfig {
    const char* lock_name;
    int num_threads;
    int iterations;
    int critical_section_us;  // Critical section duration (microseconds)
};

struct BenchResult {
    const char* lock_name;
    int num_threads;
    double throughput_ops_per_sec;
    double avg_latency_ns;
    double p50_latency_ns;
    double p99_latency_ns;
    double fairness_jain_index;
    double total_time_sec;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Busy-wait for the specified number of microseconds.
 */
static inline void busy_wait_us(int us) {
    auto start = high_resolution_clock::now();
    while (duration_cast<microseconds>(high_resolution_clock::now() - start).count() < us) {
        // Busy wait
    }
}

/**
 * @brief Calculate Jain's fairness index.
 *
 * Result ranges from 1/n (worst) to 1.0 (perfect fairness).
 */
static double calculate_fairness(const std::vector<uint64_t>& per_thread_ops) {
    if (per_thread_ops.empty()) return 0.0;

    double sum = 0, sum_sq = 0;
    for (uint64_t ops : per_thread_ops) {
        sum += ops;
        sum_sq += static_cast<double>(ops) * ops;
    }

    size_t n = per_thread_ops.size();
    return (sum * sum) / (n * sum_sq);
}

/**
 * @brief Calculate percentile from sorted latency vector.
 */
static double percentile(const std::vector<double>& sorted_latencies, double p) {
    if (sorted_latencies.empty()) return 0.0;

    size_t idx = static_cast<size_t>(sorted_latencies.size() * p / 100.0);
    if (idx >= sorted_latencies.size()) idx = sorted_latencies.size() - 1;

    return sorted_latencies[idx];
}

// ============================================================================
// Lock Benchmark Templates
// ============================================================================

/**
 * @brief Benchmark a spinlock (Ticket or MCS).
 */
template<typename LockType>
BenchResult benchmark_spinlock(const BenchConfig& config) {
    LockType lock;
    std::atomic<uint64_t> total_ops{0};
    std::vector<uint64_t> per_thread_ops(config.num_threads, 0);
    std::vector<std::vector<double>> per_thread_latencies(config.num_threads);

    auto worker = [&](int thread_id) {
        uint64_t ops = 0;
        std::vector<double>& latencies = per_thread_latencies[thread_id];
        latencies.reserve(config.iterations);

        for (int i = 0; i < config.iterations; i++) {
            auto start = high_resolution_clock::now();

            lock.lock();

            // Critical section
            if (config.critical_section_us > 0) {
                busy_wait_us(config.critical_section_us);
            }

            lock.unlock();

            auto end = high_resolution_clock::now();
            double latency = duration_cast<nanoseconds>(end - start).count();
            latencies.push_back(latency);
            ops++;
        }

        per_thread_ops[thread_id] = ops;
        total_ops += ops;
    };

    // Run benchmark
    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < config.num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = high_resolution_clock::now();
    double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

    // Aggregate latencies
    std::vector<double> all_latencies;
    for (const auto& thread_lats : per_thread_latencies) {
        all_latencies.insert(all_latencies.end(), thread_lats.begin(), thread_lats.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    // Calculate metrics
    BenchResult result;
    result.lock_name = config.lock_name;
    result.num_threads = config.num_threads;
    result.throughput_ops_per_sec = total_ops / elapsed_sec;
    result.total_time_sec = elapsed_sec;

    double sum = 0;
    for (double lat : all_latencies) sum += lat;
    result.avg_latency_ns = sum / all_latencies.size();
    result.p50_latency_ns = percentile(all_latencies, 50);
    result.p99_latency_ns = percentile(all_latencies, 99);
    result.fairness_jain_index = calculate_fairness(per_thread_ops);

    return result;
}

/**
 * @brief Benchmark MCS spinlock (requires per-thread node).
 */
BenchResult benchmark_mcs(const BenchConfig& config) {
    MCSSpinlock lock;
    std::atomic<uint64_t> total_ops{0};
    std::vector<uint64_t> per_thread_ops(config.num_threads, 0);
    std::vector<std::vector<double>> per_thread_latencies(config.num_threads);

    auto worker = [&](int thread_id) {
        MCSNode my_node;  // Per-thread node
        uint64_t ops = 0;
        std::vector<double>& latencies = per_thread_latencies[thread_id];
        latencies.reserve(config.iterations);

        for (int i = 0; i < config.iterations; i++) {
            auto start = high_resolution_clock::now();

            lock.lock(&my_node);

            if (config.critical_section_us > 0) {
                busy_wait_us(config.critical_section_us);
            }

            lock.unlock(&my_node);

            auto end = high_resolution_clock::now();
            double latency = duration_cast<nanoseconds>(end - start).count();
            latencies.push_back(latency);
            ops++;
        }

        per_thread_ops[thread_id] = ops;
        total_ops += ops;
    };

    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < config.num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = high_resolution_clock::now();
    double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

    std::vector<double> all_latencies;
    for (const auto& thread_lats : per_thread_latencies) {
        all_latencies.insert(all_latencies.end(), thread_lats.begin(), thread_lats.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    BenchResult result;
    result.lock_name = config.lock_name;
    result.num_threads = config.num_threads;
    result.throughput_ops_per_sec = total_ops / elapsed_sec;
    result.total_time_sec = elapsed_sec;

    double sum = 0;
    for (double lat : all_latencies) sum += lat;
    result.avg_latency_ns = sum / all_latencies.size();
    result.p50_latency_ns = percentile(all_latencies, 50);
    result.p99_latency_ns = percentile(all_latencies, 99);
    result.fairness_jain_index = calculate_fairness(per_thread_ops);

    return result;
}

/**
 * @brief Benchmark AdaptiveMutex (requires PID).
 */
BenchResult benchmark_adaptive(const BenchConfig& config) {
    AdaptiveMutex lock;
    std::atomic<uint64_t> total_ops{0};
    std::vector<uint64_t> per_thread_ops(config.num_threads, 0);
    std::vector<std::vector<double>> per_thread_latencies(config.num_threads);

    auto worker = [&](int thread_id) {
        xinim::pid_t my_pid = static_cast<xinim::pid_t>(thread_id + 100);
        uint64_t ops = 0;
        std::vector<double>& latencies = per_thread_latencies[thread_id];
        latencies.reserve(config.iterations);

        for (int i = 0; i < config.iterations; i++) {
            auto start = high_resolution_clock::now();

            lock.lock(my_pid);

            if (config.critical_section_us > 0) {
                busy_wait_us(config.critical_section_us);
            }

            lock.unlock(my_pid);

            auto end = high_resolution_clock::now();
            double latency = duration_cast<nanoseconds>(end - start).count();
            latencies.push_back(latency);
            ops++;
        }

        per_thread_ops[thread_id] = ops;
        total_ops += ops;
    };

    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < config.num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = high_resolution_clock::now();
    double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

    std::vector<double> all_latencies;
    for (const auto& thread_lats : per_thread_latencies) {
        all_latencies.insert(all_latencies.end(), thread_lats.begin(), thread_lats.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    BenchResult result;
    result.lock_name = config.lock_name;
    result.num_threads = config.num_threads;
    result.throughput_ops_per_sec = total_ops / elapsed_sec;
    result.total_time_sec = elapsed_sec;

    double sum = 0;
    for (double lat : all_latencies) sum += lat;
    result.avg_latency_ns = sum / all_latencies.size();
    result.p50_latency_ns = percentile(all_latencies, 50);
    result.p99_latency_ns = percentile(all_latencies, 99);
    result.fairness_jain_index = calculate_fairness(per_thread_ops);

    return result;
}

/**
 * @brief Benchmark PhaseRWLock (write-only test).
 */
BenchResult benchmark_phase_rwlock_write(const BenchConfig& config) {
    PhaseRWLock lock;
    std::atomic<uint64_t> total_ops{0};
    std::vector<uint64_t> per_thread_ops(config.num_threads, 0);
    std::vector<std::vector<double>> per_thread_latencies(config.num_threads);

    auto worker = [&](int thread_id) {
        uint64_t ops = 0;
        std::vector<double>& latencies = per_thread_latencies[thread_id];
        latencies.reserve(config.iterations);

        for (int i = 0; i < config.iterations; i++) {
            auto start = high_resolution_clock::now();

            lock.write_lock();

            if (config.critical_section_us > 0) {
                busy_wait_us(config.critical_section_us);
            }

            lock.write_unlock();

            auto end = high_resolution_clock::now();
            double latency = duration_cast<nanoseconds>(end - start).count();
            latencies.push_back(latency);
            ops++;
        }

        per_thread_ops[thread_id] = ops;
        total_ops += ops;
    };

    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < config.num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = high_resolution_clock::now();
    double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

    std::vector<double> all_latencies;
    for (const auto& thread_lats : per_thread_latencies) {
        all_latencies.insert(all_latencies.end(), thread_lats.begin(), thread_lats.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    BenchResult result;
    result.lock_name = config.lock_name;
    result.num_threads = config.num_threads;
    result.throughput_ops_per_sec = total_ops / elapsed_sec;
    result.total_time_sec = elapsed_sec;

    double sum = 0;
    for (double lat : all_latencies) sum += lat;
    result.avg_latency_ns = sum / all_latencies.size();
    result.p50_latency_ns = percentile(all_latencies, 50);
    result.p99_latency_ns = percentile(all_latencies, 99);
    result.fairness_jain_index = calculate_fairness(per_thread_ops);

    return result;
}

/**
 * @brief Benchmark CapabilityMutex (requires PID and capability).
 */
BenchResult benchmark_capability(const BenchConfig& config) {
    CapabilityMutex lock;
    std::atomic<uint64_t> total_ops{0};
    std::vector<uint64_t> per_thread_ops(config.num_threads, 0);
    std::vector<std::vector<double>> per_thread_latencies(config.num_threads);

    auto worker = [&](int thread_id) {
        xinim::pid_t my_pid = static_cast<xinim::pid_t>(thread_id + 200);
        CapabilityToken token{static_cast<uint64_t>(my_pid), 0xCAFEBABE};

        uint64_t ops = 0;
        std::vector<double>& latencies = per_thread_latencies[thread_id];
        latencies.reserve(config.iterations);

        for (int i = 0; i < config.iterations; i++) {
            auto start = high_resolution_clock::now();

            while (!lock.lock(my_pid, token)) {
                // Retry if capability verification fails
            }

            if (config.critical_section_us > 0) {
                busy_wait_us(config.critical_section_us);
            }

            lock.unlock(my_pid);

            auto end = high_resolution_clock::now();
            double latency = duration_cast<nanoseconds>(end - start).count();
            latencies.push_back(latency);
            ops++;
        }

        per_thread_ops[thread_id] = ops;
        total_ops += ops;
    };

    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < config.num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = high_resolution_clock::now();
    double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

    std::vector<double> all_latencies;
    for (const auto& thread_lats : per_thread_latencies) {
        all_latencies.insert(all_latencies.end(), thread_lats.begin(), thread_lats.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    BenchResult result;
    result.lock_name = config.lock_name;
    result.num_threads = config.num_threads;
    result.throughput_ops_per_sec = total_ops / elapsed_sec;
    result.total_time_sec = elapsed_sec;

    double sum = 0;
    for (double lat : all_latencies) sum += lat;
    result.avg_latency_ns = sum / all_latencies.size();
    result.p50_latency_ns = percentile(all_latencies, 50);
    result.p99_latency_ns = percentile(all_latencies, 99);
    result.fairness_jain_index = calculate_fairness(per_thread_ops);

    return result;
}

// ============================================================================
// Results Display
// ============================================================================

void print_header() {
    std::cout << std::left
              << std::setw(20) << "Lock Type"
              << std::setw(10) << "Threads"
              << std::setw(15) << "Throughput"
              << std::setw(12) << "Avg Lat"
              << std::setw(12) << "P50 Lat"
              << std::setw(12) << "P99 Lat"
              << std::setw(10) << "Fairness"
              << std::setw(10) << "Time"
              << std::endl;

    std::cout << std::string(101, '-') << std::endl;
}

void print_result(const BenchResult& r) {
    std::cout << std::left
              << std::setw(20) << r.lock_name
              << std::setw(10) << r.num_threads
              << std::setw(15) << (std::to_string(static_cast<int>(r.throughput_ops_per_sec / 1e6)) + " M/s")
              << std::setw(12) << (std::to_string(static_cast<int>(r.avg_latency_ns)) + " ns")
              << std::setw(12) << (std::to_string(static_cast<int>(r.p50_latency_ns)) + " ns")
              << std::setw(12) << (std::to_string(static_cast<int>(r.p99_latency_ns)) + " ns")
              << std::setw(10) << std::fixed << std::setprecision(3) << r.fairness_jain_index
              << std::setw(10) << std::fixed << std::setprecision(2) << r.total_time_sec << "s"
              << std::endl;
}

void print_separator() {
    std::cout << std::string(101, '-') << std::endl;
}

// ============================================================================
// Main Benchmark Runner
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "XINIM Lock Framework Benchmark Suite" << std::endl;
    std::cout << "=====================================" << std::endl << std::endl;

    // Configuration
    int iterations = (argc > 1) ? std::atoi(argv[1]) : 100000;
    int critical_section_us = (argc > 2) ? std::atoi(argv[2]) : 1;

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Critical section: " << critical_section_us << " us" << std::endl;
    std::cout << "  Hardware threads: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << std::endl;

    std::vector<BenchResult> results;

    // Test thread counts
    int thread_counts[] = {1, 2, 4, 8, 16};

    print_header();

    for (int num_threads : thread_counts) {
        if (num_threads > static_cast<int>(std::thread::hardware_concurrency())) {
            continue; // Skip if more threads than cores
        }

        std::cout << "\n## " << num_threads << " Thread" << (num_threads > 1 ? "s" : "") << std::endl;
        print_separator();

        // Benchmark TicketSpinlock
        auto r1 = benchmark_spinlock<TicketSpinlock>({
            "TicketSpinlock", num_threads, iterations, critical_section_us
        });
        print_result(r1);
        results.push_back(r1);

        // Benchmark MCSSpinlock
        auto r2 = benchmark_mcs({
            "MCSSpinlock", num_threads, iterations, critical_section_us
        });
        print_result(r2);
        results.push_back(r2);

        // Benchmark AdaptiveMutex
        auto r3 = benchmark_adaptive({
            "AdaptiveMutex", num_threads, iterations, critical_section_us
        });
        print_result(r3);
        results.push_back(r3);

        // Benchmark PhaseRWLock (write)
        auto r4 = benchmark_phase_rwlock_write({
            "PhaseRWLock(W)", num_threads, iterations, critical_section_us
        });
        print_result(r4);
        results.push_back(r4);

        // Benchmark CapabilityMutex
        auto r5 = benchmark_capability({
            "CapabilityMutex", num_threads, iterations, critical_section_us
        });
        print_result(r5);
        results.push_back(r5);
    }

    print_separator();

    // Summary
    std::cout << "\nSummary:" << std::endl;
    std::cout << "  Total benchmarks: " << results.size() << std::endl;
    std::cout << "  Lock types: 5 (Ticket, MCS, Adaptive, PhaseRW, Capability)" << std::endl;
    std::cout << "  Thread counts tested: ";
    for (int tc : thread_counts) {
        if (tc <= static_cast<int>(std::thread::hardware_concurrency())) {
            std::cout << tc << " ";
        }
    }
    std::cout << std::endl;

    std::cout << "\nKey Insights:" << std::endl;
    std::cout << "  - TicketSpinlock: FIFO fairness, good for low contention" << std::endl;
    std::cout << "  - MCSSpinlock: Best scaling, NUMA-aware" << std::endl;
    std::cout << "  - AdaptiveMutex: Spin-then-sleep, good for variable workloads" << std::endl;
    std::cout << "  - PhaseRWLock: Phase-fair, prevents reader/writer starvation" << std::endl;
    std::cout << "  - CapabilityMutex: Crash recovery, capability-based security" << std::endl;

    return 0;
}
