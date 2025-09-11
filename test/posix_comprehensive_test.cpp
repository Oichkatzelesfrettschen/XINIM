/**
 * @file posix_comprehensive_test.cpp
 * @brief Comprehensive POSIX compliance test for XINIM
 */

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <semaphore>
#include <chrono>
#include <filesystem>
#include <sys/mman.h>
#include <signal.h>

// Include our POSIX header
#include <xinim/posix.hpp>

using namespace std::chrono_literals;

class PosixComplianceTester {
private:
    int passed_tests = 0;
    int total_tests = 0;

    void test_result(bool result, const std::string& test_name) {
        total_tests++;
        if (result) {
            passed_tests++;
            std::cout << "   ✓ " << test_name << "\n";
        } else {
            std::cout << "   ✗ " << test_name << "\n";
        }
    }

public:
    void run_all_tests() {
        std::cout << "XINIM Comprehensive POSIX Compliance Test\n";
        std::cout << "=========================================\n\n";

        test_process_management();
        test_file_operations();
        test_threading();
        test_synchronization();
        test_time_functions();
        test_memory_management();
        test_signals();
        test_networking();
        test_message_queues();
        test_semaphores();
        test_scheduling();

        std::cout << "\n=========================================\n";
        std::cout << "Test Results: " << passed_tests << "/" << total_tests << " passed\n";
        std::cout << "Compliance: " << (passed_tests * 100.0 / total_tests) << "%\n";
    }

private:
    void test_process_management() {
        std::cout << "1. Process Management Tests\n";

        // Test getpid
        auto pid_result = xinim::posix::process::getpid();
        test_result(pid_result.has_value(), "getpid() returns valid PID");

        // Test getppid
        auto ppid_result = xinim::posix::process::getppid();
        test_result(ppid_result.has_value(), "getppid() returns valid PPID");

        // Test fork (careful - this creates a new process)
        auto fork_result = xinim::posix::process::fork();
        if (fork_result) {
            if (*fork_result == 0) {
                // Child process
                exit(0);
            } else {
                // Parent process - wait for child
                auto wait_result = xinim::posix::process::waitpid(*fork_result, 0);
                test_result(wait_result.has_value(), "fork() and waitpid() work correctly");
            }
        } else {
            test_result(false, "fork() failed: " + fork_result.error().message());
        }
    }

    void test_file_operations() {
        std::cout << "\n2. File Operations Tests\n";

        const std::string test_file = "/tmp/xinim_posix_test.txt";
        const std::string test_data = "Hello from XINIM POSIX test!";

        // Test file creation and writing
        auto fd_result = xinim::posix::file::open(test_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        test_result(fd_result.has_value(), "File creation with open()");

        if (fd_result) {
            std::span<const std::byte> data_span(
                reinterpret_cast<const std::byte*>(test_data.data()),
                test_data.size()
            );

            auto write_result = xinim::posix::file::write(fd_result->get(), data_span);
            test_result(write_result.has_value() && *write_result == test_data.size(),
                       "File writing with write()");

            auto close_result = xinim::posix::file::close(fd_result->get());
            test_result(close_result.has_value(), "File closing with close()");
        }

        // Test file reading
        auto read_fd_result = xinim::posix::file::open(test_file, O_RDONLY);
        test_result(read_fd_result.has_value(), "File opening for reading");

        if (read_fd_result) {
            std::string read_buffer(test_data.size(), '\0');
            std::span<std::byte> read_span(
                reinterpret_cast<std::byte*>(read_buffer.data()),
                read_buffer.size()
            );

            auto read_result = xinim::posix::file::read(read_fd_result->get(), read_span);
            test_result(read_result.has_value() && *read_result == test_data.size() &&
                       read_buffer == test_data, "File reading with read()");

            xinim::posix::file::close(read_fd_result->get());
        }

        // Cleanup
        std::filesystem::remove(test_file);
    }

    void test_threading() {
        std::cout << "\n3. Threading Tests\n";

        static std::mutex thread_test_mutex;
        static bool thread_ran = false;

        pthread_t thread;
        auto create_result = xinim::posix::thread::pthread_create(&thread, nullptr, [](void*) -> void* {
            std::lock_guard<std::mutex> lock(thread_test_mutex);
            thread_ran = true;
            return nullptr;
        }, nullptr);

        test_result(create_result.has_value(), "Thread creation with pthread_create()");

        if (create_result) {
            auto join_result = xinim::posix::thread::pthread_join(thread, nullptr);
            test_result(join_result.has_value() && thread_ran, "Thread joining with pthread_join()");
        }
    }

    void test_synchronization() {
        std::cout << "\n4. Synchronization Tests\n";

        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        auto init_result = xinim::posix::sync::pthread_mutex_init(&mutex, nullptr);
        test_result(init_result.has_value(), "Mutex initialization");

        if (init_result) {
            auto lock_result = xinim::posix::sync::pthread_mutex_lock(&mutex);
            test_result(lock_result.has_value(), "Mutex locking");

            auto unlock_result = xinim::posix::sync::pthread_mutex_unlock(&mutex);
            test_result(unlock_result.has_value(), "Mutex unlocking");

            auto destroy_result = xinim::posix::sync::pthread_mutex_destroy(&mutex);
            test_result(destroy_result.has_value(), "Mutex destruction");
        }
    }

    void test_time_functions() {
        std::cout << "\n5. Time Functions Tests\n";

        struct timespec ts;
        auto time_result = xinim::posix::time::clock_gettime(CLOCK_REALTIME, &ts);
        test_result(time_result.has_value() && ts.tv_sec > 0, "clock_gettime() with CLOCK_REALTIME");

        auto mono_result = xinim::posix::time::clock_gettime(CLOCK_MONOTONIC, &ts);
        test_result(mono_result.has_value() && ts.tv_sec >= 0, "clock_gettime() with CLOCK_MONOTONIC");

        // Test nanosleep
        struct timespec sleep_time = {0, 1000000}; // 1ms
        struct timespec remaining;
        auto sleep_result = xinim::posix::time::nanosleep(&sleep_time, &remaining);
        test_result(sleep_result.has_value(), "nanosleep() for 1ms");
    }

    void test_memory_management() {
        std::cout << "\n6. Memory Management Tests\n";

        const size_t test_size = 4096;
        auto mmap_result = xinim::posix::memory::mmap(nullptr, test_size,
                                                     PROT_READ | PROT_WRITE,
                                                     MAP_PRIVATE | MAP_ANONYMOUS,
                                                     -1, 0);
        test_result(mmap_result.has_value() && *mmap_result != MAP_FAILED,
                   "Memory mapping with mmap()");

        if (mmap_result && *mmap_result != MAP_FAILED) {
            // Test memory access
            auto* ptr = static_cast<char*>(*mmap_result);
            ptr[0] = 'X';
            test_result(ptr[0] == 'X', "Memory access after mmap()");

            auto munmap_result = xinim::posix::memory::munmap(*mmap_result, test_size);
            test_result(munmap_result.has_value(), "Memory unmapping with munmap()");
        }
    }

    void test_signals() {
        std::cout << "\n7. Signal Tests\n";

        // Test signal sending to self (safe)
        auto kill_result = xinim::posix::signal::kill(getpid(), 0); // Signal 0 doesn't do anything
        test_result(kill_result.has_value(), "Signal sending with kill() (signal 0)");
    }

    void test_networking() {
        std::cout << "\n8. Networking Tests\n";

        // Test socket creation
        auto socket_result = xinim::posix::network::socket(AF_INET, SOCK_STREAM, 0);
        test_result(socket_result.has_value(), "TCP socket creation");

        if (socket_result) {
            auto close_result = xinim::posix::file::close(socket_result->get());
            test_result(close_result.has_value(), "Socket closing");
        }

        // Test UDP socket
        auto udp_socket_result = xinim::posix::network::socket(AF_INET, SOCK_DGRAM, 0);
        test_result(udp_socket_result.has_value(), "UDP socket creation");

        if (udp_socket_result) {
            xinim::posix::file::close(udp_socket_result->get());
        }
    }

    void test_message_queues() {
        std::cout << "\n9. Message Queue Tests\n";

        const char* queue_name = "/xinim_test_queue";
        struct mq_attr attr = {0, 10, 256, 0};

        auto mq_result = xinim::posix::mq::mq_open(queue_name, O_CREAT | O_RDWR, 0644, &attr);
        test_result(mq_result.has_value(), "Message queue creation with mq_open()");

        if (mq_result) {
            const char* test_msg = "Hello MQ!";
            auto send_result = xinim::posix::mq::mq_send(*mq_result, test_msg, strlen(test_msg), 0);
            test_result(send_result.has_value(), "Message sending with mq_send()");

            // Add a small delay to ensure message is sent
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            char recv_buf[256] = {0};
            unsigned int prio = 0;
            auto recv_result = xinim::posix::mq::mq_receive(*mq_result, recv_buf, sizeof(recv_buf), &prio);
            if (!recv_result.has_value()) {
                std::cout << "   mq_receive failed with error: " << recv_result.error().message() << " (errno: " << recv_result.error().value() << ")\n";
            }
            test_result(recv_result.has_value() && strcmp(recv_buf, test_msg) == 0,
                       "Message receiving with mq_receive()");

            auto close_result = xinim::posix::mq::mq_close(*mq_result);
            test_result(close_result.has_value(), "Message queue closing with mq_close()");

            auto unlink_result = xinim::posix::mq::mq_unlink(queue_name);
            test_result(unlink_result.has_value(), "Message queue unlinking with mq_unlink()");
        }
    }

    void test_semaphores() {
        std::cout << "\n10. Semaphore Tests\n";

        sem_t sem;
        auto init_result = xinim::posix::semaphore::sem_init(&sem, 0, 1);
        test_result(init_result.has_value(), "Semaphore initialization with sem_init()");

        if (init_result) {
            auto wait_result = xinim::posix::semaphore::sem_wait(&sem);
            test_result(wait_result.has_value(), "Semaphore wait with sem_wait()");

            auto post_result = xinim::posix::semaphore::sem_post(&sem);
            test_result(post_result.has_value(), "Semaphore post with sem_post()");

            auto destroy_result = xinim::posix::semaphore::sem_destroy(&sem);
            test_result(destroy_result.has_value(), "Semaphore destruction with sem_destroy()");
        }
    }

    void test_scheduling() {
        std::cout << "\n11. Scheduling Tests\n";

        auto min_prio = xinim::posix::sched::sched_get_priority_min(SCHED_OTHER);
        test_result(min_prio.has_value(), "Getting minimum priority with sched_get_priority_min()");

        auto max_prio = xinim::posix::sched::sched_get_priority_max(SCHED_OTHER);
        test_result(max_prio.has_value(), "Getting maximum priority with sched_get_priority_max()");

        auto yield_result = xinim::posix::sched::sched_yield();
        test_result(yield_result.has_value(), "CPU yielding with sched_yield()");
    }
};

int main() {
    PosixComplianceTester tester;
    tester.run_all_tests();
    return 0;
}
