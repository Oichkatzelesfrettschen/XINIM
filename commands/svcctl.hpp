#pragma once
/**
 * @file svcctl.hpp
 * @brief Interface for the svcctl utility used in tests and as a command.
 */

#include "../include/xinim/core_types.hpp"
#include <span>

namespace svcctl {

/** Message opcodes understood by the mock service manager. */
enum class Message : int {
    LIST = 1,          //!< Request list of services
    START = 2,         //!< Start a service
    STOP = 3,          //!< Stop a service
    RESTART = 4,       //!< Restart a service
    LIST_RESPONSE = 5, //!< Single service entry in response
    ACK = 6,           //!< Generic acknowledgement
    END = 7,           //!< End of list indicator
    SHUTDOWN = 8       //!< Terminate the manager thread
};

inline constexpr xinim::pid_t CLIENT_PID = 200; ///< PID used by svcctl
inline constexpr xinim::pid_t MANAGER_PID = 1;  ///< PID for the service manager

/**
 * @brief Execute the svcctl command with @p args.
 *
 * The argument span should mirror the parameters normally passed to main().
 * This function is provided so unit tests can invoke the command directly
 * without spawning a new process.
 *
 * @param args Command-line style argument span.
 * @return Exit status compatible with POSIX conventions.
 */
int run(std::span<char *> args);

} // namespace svcctl
