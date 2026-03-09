/**
 * @file posix_test.cpp
 * @brief Basic POSIX compliance test for XINIM
 */

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

// Include our POSIX header
#include <xinim/posix.hpp>

int main() {
    std::cout << "XINIM POSIX Compliance Test\n";
    std::cout << "===========================\n\n";

    // Test 1: Process ID functions
    std::cout << "1. Testing process functions...\n";
    auto pid_result = xinim::posix::process::getpid();
    if (pid_result) {
        std::cout << "   ✓ getpid() returned: " << *pid_result << "\n";
    } else {
        std::cout << "   ✗ getpid() failed: " << pid_result.error().message() << "\n";
    }

    auto ppid_result = xinim::posix::process::getppid();
    if (ppid_result) {
        std::cout << "   ✓ getppid() returned: " << *ppid_result << "\n";
    } else {
        std::cout << "   ✗ getppid() failed: " << ppid_result.error().message() << "\n";
    }

    // Test 2: Time functions
    std::cout << "\n2. Testing time functions...\n";
    struct timespec ts;
    auto time_result = xinim::posix::time::clock_gettime(CLOCK_REALTIME, &ts);
    if (time_result) {
        std::cout << "   ✓ clock_gettime() succeeded\n";
        std::cout << "   Current time: " << ts.tv_sec << "." << ts.tv_nsec << " seconds\n";
    } else {
        std::cout << "   ✗ clock_gettime() failed: " << time_result.error().message() << "\n";
    }

    // Test 3: File operations
    std::cout << "\n3. Testing file operations...\n";
    auto fd_result = xinim::posix::file::open("/tmp/posix_test.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd_result) {
        std::cout << "   ✓ File opened successfully\n";

        std::string test_data = "Hello, POSIX from XINIM!";
        std::span<const std::byte> data_span(reinterpret_cast<const std::byte*>(test_data.data()), test_data.size());

        auto write_result = xinim::posix::file::write(fd_result->get(), data_span);
        if (write_result) {
            std::cout << "   ✓ Wrote " << *write_result << " bytes to file\n";
        } else {
            std::cout << "   ✗ Write failed: " << write_result.error().message() << "\n";
        }

        auto close_result = xinim::posix::file::close(fd_result->get());
        if (close_result) {
            std::cout << "   ✓ File closed successfully\n";
        } else {
            std::cout << "   ✗ Close failed: " << close_result.error().message() << "\n";
        }
    } else {
        std::cout << "   ✗ File open failed: " << fd_result.error().message() << "\n";
    }

    // Test 4: Scheduling functions
    std::cout << "\n4. Testing scheduling functions...\n";
    auto min_prio = xinim::posix::sched::sched_get_priority_min(SCHED_OTHER);
    if (min_prio) {
        std::cout << "   ✓ Minimum priority for SCHED_OTHER: " << *min_prio << "\n";
    } else {
        std::cout << "   ✗ sched_get_priority_min() failed: " << min_prio.error().message() << "\n";
    }

    auto max_prio = xinim::posix::sched::sched_get_priority_max(SCHED_OTHER);
    if (max_prio) {
        std::cout << "   ✓ Maximum priority for SCHED_OTHER: " << *max_prio << "\n";
    } else {
        std::cout << "   ✗ sched_get_priority_max() failed: " << max_prio.error().message() << "\n";
    }

    // Test 5: Yield
    std::cout << "\n5. Testing sched_yield()...\n";
    auto yield_result = xinim::posix::sched::sched_yield();
    if (yield_result) {
        std::cout << "   ✓ sched_yield() succeeded\n";
    } else {
        std::cout << "   ✗ sched_yield() failed: " << yield_result.error().message() << "\n";
    }

    std::cout << "\nPOSIX compliance test completed!\n";
    return 0;
}
