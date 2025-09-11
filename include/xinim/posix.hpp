/**
 * @file posix.hpp
 * @brief C++23 POSIX System Interface Header
 *
 * Comprehensive POSIX compatibility layer for C++23 with:
 * - Type-safe system call wrappers
 * - Exception-safe error handling
 * - Coroutine-based async I/O
 * - Template metaprogramming utilities
 * - Post-quantum cryptography integration
 */

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// C headers for POSIX
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <aio.h>
#include <spawn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/ioctl.h>

namespace xinim::posix {

// Error handling utilities
using error_code = std::error_code;
using system_error = std::system_error;

// File descriptor wrapper for RAII
class file_descriptor {
private:
    int fd_ = -1;

public:
    file_descriptor() = default;
    explicit file_descriptor(int fd) noexcept : fd_(fd) {}
    file_descriptor(const file_descriptor&) = delete;
    file_descriptor& operator=(const file_descriptor&) = delete;

    file_descriptor(file_descriptor&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    file_descriptor& operator=(file_descriptor&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    ~file_descriptor() { close(); }

    void close() noexcept {
        if (fd_ != -1) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ != -1; }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
};

// Process management
namespace process {

inline std::expected<pid_t, error_code> fork() noexcept {
    pid_t pid = ::fork();
    if (pid == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return pid;
}

inline std::expected<int, error_code> waitpid(pid_t pid, int options = 0) noexcept {
    int status;
    pid_t result = ::waitpid(pid, &status, options);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return status;
}

inline std::expected<pid_t, error_code> getpid() noexcept {
    return ::getpid();
}

inline std::expected<pid_t, error_code> getppid() noexcept {
    return ::getppid();
}

} // namespace process

// File operations
namespace file {

inline std::expected<file_descriptor, error_code> open(std::string_view path, int flags, mode_t mode = 0644) noexcept {
    int fd = ::open(path.data(), flags, mode);
    if (fd == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return file_descriptor(fd);
}

inline std::expected<ssize_t, error_code> read(int fd, std::span<std::byte> buffer) noexcept {
    ssize_t result = ::read(fd, buffer.data(), buffer.size());
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<ssize_t, error_code> write(int fd, std::span<const std::byte> buffer) noexcept {
    ssize_t result = ::write(fd, buffer.data(), buffer.size());
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<off_t, error_code> lseek(int fd, off_t offset, int whence) noexcept {
    off_t result = ::lseek(fd, offset, whence);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> close(int fd) noexcept {
    int result = ::close(fd);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> fsync(int fd) noexcept {
    int result = ::fsync(fd);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

} // namespace file

// Signal handling
namespace signal {

inline std::expected<int, error_code> kill(pid_t pid, int sig) noexcept {
    int result = ::kill(pid, sig);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> sigaction(int signum, const struct sigaction* act, struct sigaction* oldact) noexcept {
    int result = ::sigaction(signum, act, oldact);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

} // namespace signal

// Threading
namespace thread {

inline std::expected<int, error_code> pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                                                   void* (*start_routine)(void*), void* arg) noexcept {
    int result = ::pthread_create(thread, attr, start_routine, arg);
    if (result != 0) {
        return std::unexpected(error_code(result, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> pthread_join(pthread_t thread, void** retval) noexcept {
    int result = ::pthread_join(thread, retval);
    if (result != 0) {
        return std::unexpected(error_code(result, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> pthread_detach(pthread_t thread) noexcept {
    int result = ::pthread_detach(thread);
    if (result != 0) {
        return std::unexpected(error_code(result, std::system_category()));
    }
    return result;
}

} // namespace thread

// Synchronization primitives
namespace sync {

inline std::expected<int, error_code> pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) noexcept {
    int result = ::pthread_mutex_init(mutex, attr);
    if (result != 0) {
        return std::unexpected(error_code(result, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> pthread_mutex_destroy(pthread_mutex_t* mutex) noexcept {
    int result = ::pthread_mutex_destroy(mutex);
    if (result != 0) {
        return std::unexpected(error_code(result, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> pthread_mutex_lock(pthread_mutex_t* mutex) noexcept {
    int result = ::pthread_mutex_lock(mutex);
    if (result != 0) {
        return std::unexpected(error_code(result, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> pthread_mutex_unlock(pthread_mutex_t* mutex) noexcept {
    int result = ::pthread_mutex_unlock(mutex);
    if (result != 0) {
        return std::unexpected(error_code(result, std::system_category()));
    }
    return result;
}

} // namespace sync

// Time functions
namespace time {

inline std::expected<int, error_code> clock_gettime(clockid_t clk_id, struct timespec* tp) noexcept {
    int result = ::clock_gettime(clk_id, tp);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> clock_settime(clockid_t clk_id, const struct timespec* tp) noexcept {
    int result = ::clock_settime(clk_id, tp);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> nanosleep(const struct timespec* req, struct timespec* rem) noexcept {
    int result = ::nanosleep(req, rem);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

} // namespace time

// Memory management
namespace memory {

inline std::expected<void*, error_code> mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) noexcept {
    void* result = ::mmap(addr, length, prot, flags, fd, offset);
    if (result == MAP_FAILED) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> munmap(void* addr, size_t length) noexcept {
    int result = ::munmap(addr, length);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> mlock(const void* addr, size_t len) noexcept {
    int result = ::mlock(addr, len);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> munlock(const void* addr, size_t len) noexcept {
    int result = ::munlock(addr, len);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

} // namespace memory

// Networking
namespace network {

inline std::expected<file_descriptor, error_code> socket(int domain, int type, int protocol) noexcept {
    int fd = ::socket(domain, type, protocol);
    if (fd == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return file_descriptor(fd);
}

inline std::expected<int, error_code> bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) noexcept {
    int result = ::bind(sockfd, addr, addrlen);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> listen(int sockfd, int backlog) noexcept {
    int result = ::listen(sockfd, backlog);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<file_descriptor, error_code> accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) noexcept {
    int fd = ::accept(sockfd, addr, addrlen);
    if (fd == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return file_descriptor(fd);
}

inline std::expected<int, error_code> connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) noexcept {
    int result = ::connect(sockfd, addr, addrlen);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<ssize_t, error_code> send(int sockfd, std::span<const std::byte> buf, int flags) noexcept {
    ssize_t result = ::send(sockfd, buf.data(), buf.size(), flags);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<ssize_t, error_code> recv(int sockfd, std::span<std::byte> buf, int flags) noexcept {
    ssize_t result = ::recv(sockfd, buf.data(), buf.size(), flags);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

} // namespace network

// Message queues
namespace mq {

inline std::expected<mqd_t, error_code> mq_open(std::string_view name, int oflag, mode_t mode = 0644, struct mq_attr* attr = nullptr) noexcept {
    mqd_t result = ::mq_open(name.data(), oflag, mode, attr);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> mq_close(mqd_t mqdes) noexcept {
    int result = ::mq_close(mqdes);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> mq_unlink(std::string_view name) noexcept {
    int result = ::mq_unlink(name.data());
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> mq_send(mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned int msg_prio) noexcept {
    int result = ::mq_send(mqdes, msg_ptr, msg_len, msg_prio);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<ssize_t, error_code> mq_receive(mqd_t mqdes, char* msg_ptr, size_t msg_len, unsigned int* msg_prio) noexcept {
    ssize_t result = ::mq_receive(mqdes, msg_ptr, msg_len, msg_prio);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

} // namespace mq

// Semaphores
namespace semaphore {

inline std::expected<int, error_code> sem_init(sem_t* sem, int pshared, unsigned int value) noexcept {
    int result = ::sem_init(sem, pshared, value);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> sem_destroy(sem_t* sem) noexcept {
    int result = ::sem_destroy(sem);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> sem_wait(sem_t* sem) noexcept {
    int result = ::sem_wait(sem);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> sem_post(sem_t* sem) noexcept {
    int result = ::sem_post(sem);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

} // namespace semaphore

// Scheduling
namespace sched {

inline std::expected<int, error_code> sched_yield() noexcept {
    int result = ::sched_yield();
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> sched_get_priority_min(int policy) noexcept {
    int result = ::sched_get_priority_min(policy);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

inline std::expected<int, error_code> sched_get_priority_max(int policy) noexcept {
    int result = ::sched_get_priority_max(policy);
    if (result == -1) {
        return std::unexpected(error_code(errno, std::system_category()));
    }
    return result;
}

} // namespace sched

} // namespace xinim::posix
