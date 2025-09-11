/**
 * @file fork.cpp
 * @brief RAII-enabled interface for process creation.
 */

#include "../include/lib.hpp" ///< Project-wide system call interface
#include <cerrno>             ///< errno for detailed error reporting
#include <sys/types.h>        ///< pid_t definition
#include <sys/wait.h>         ///< waitpid for child reaping
#include <system_error>       ///< std::system_error for exceptions

/**
 * @brief RAII wrapper owning a child process created with `fork`.
 *
 * The constructor performs the `FORK` system call via `callm1`. If the call
 * fails a `std::system_error` is thrown. In the parent process the destructor
 * waits for the child to terminate, preventing zombie processes. Child-side
 * instances do nothing on destruction.
 */
class ForkGuard {
  public:
    /**
     * @brief Spawn a child process and take ownership of its PID.
     * @throws std::system_error if the underlying `FORK` system call fails.
     */
    ForkGuard() {
        pid_ = static_cast<pid_t>(callm1(MM, FORK, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR));
        if (pid_ < 0) {
            throw std::system_error(errno, std::generic_category(), "fork failed");
        }
    }

    ForkGuard(const ForkGuard &) = delete;            ///< Non-copyable
    ForkGuard &operator=(const ForkGuard &) = delete; ///< Non-copyable

    /// Move constructor transfers ownership of the child PID.
    ForkGuard(ForkGuard &&other) noexcept : pid_(other.pid_) { other.pid_ = -1; }

    /// Move assignment transfers ownership and reaps any existing child.
    ForkGuard &operator=(ForkGuard &&other) noexcept {
        if (this != &other) {
            cleanup();
            pid_ = other.pid_;
            other.pid_ = -1;
        }
        return *this;
    }

    /// Destructor waits for the child in the parent process.
    ~ForkGuard() { cleanup(); }

    /// @return PID returned by the `fork` operation.
    [[nodiscard]] pid_t pid() const noexcept { return pid_; }

    /// @return `true` if executing in the child process.
    [[nodiscard]] bool is_child() const noexcept { return pid_ == 0; }

    /// @return `true` if executing in the parent process.
    [[nodiscard]] bool is_parent() const noexcept { return pid_ > 0; }

  private:
    pid_t pid_{-1}; ///< Owned child PID or negative on failure

    /// Wait for the child to exit if this is the parent process.
    void cleanup() noexcept {
        if (pid_ > 0) {
            int status{};
            ::waitpid(pid_, &status, 0);
            pid_ = -1;
        }
    }
};

/**
 * @brief Create a new process duplicating the calling process.
 *
 * This retains the historical interface returning a raw PID value.
 *
 * @return PID of the new process or a negative value on failure.
 */
pid_t fork() { return static_cast<pid_t>(callm1(MM, FORK, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR)); }

/**
 * @brief RAII-enabled wrapper around `fork`.
 *
 * @return ForkGuard managing the lifetime of the forked child.
 * @throws std::system_error if the underlying `fork` fails.
 */
[[nodiscard]] ForkGuard scoped_fork() { return ForkGuard{}; }
