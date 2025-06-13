/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#pragma once

// Bring in the shared signal definitions used by both kernel and user space.
#include "shared/signal_constants.hpp" // Provides SIGHUP etc., sighandler_t, signal()
#include "../../include/xinim/core_types.hpp" // For xinim::pid_t, xinim::uid_t, std::uint64_t

// Define sigset_t for signal sets
using sigset_t = std::uint64_t;

// Define a basic siginfo_t structure (can be expanded for full POSIX features)
// Using typedef for C compatibility, as this struct is part of a C ABI.
typedef struct siginfo_t {
    int            si_signo;    /* Signal number */
    int            si_errno;    /* An errno value */
    int            si_code;     /* Signal code */
    xinim::pid_t   si_pid;      /* Sending process ID */
    xinim::uid_t   si_uid;      /* Real user ID of sending process */
    void          *si_addr;     /* Address of faulting instruction */
    int            si_status;   /* Exit value or signal */
    /* union sigval si_value; // Omitted for current simplicity, TODO for full POSIX */
} siginfo_t;

// Define struct sigaction
// sighandler_t comes from shared/signal_constants.hpp
struct sigaction {
    sighandler_t sa_handler;
    sigset_t     sa_mask;
    int          sa_flags;
    void         (*sa_sigaction)(int, siginfo_t *, void *); // Modern C-style function pointer
};

// Constants for sa_flags in struct sigaction
inline constexpr int SA_NOCLDSTOP = 1;  /* Do not generate SIGCHLD when children stop. */
inline constexpr int SA_RESETHAND = 2;  /* Reset handler to SIG_DFL on entry. */
inline constexpr int SA_NODEFER   = 4;  /* Do not automatically block signal on entry. */
inline constexpr int SA_SIGINFO   = 8;  /* Use sa_sigaction field instead of sa_handler. */
// TODO: Add other flags like SA_RESTART if they are to be supported.

// Constants for the 'how' argument of sigprocmask
inline constexpr int SIG_BLOCK   = 0;  /* Block signals in set. */
inline constexpr int SIG_UNBLOCK = 1;  /* Unblock signals in set. */
inline constexpr int SIG_SETMASK = 2;  /* Set signal mask to set. */

// Standard signal function declarations
extern "C" {

// signal() is already declared in shared/signal_constants.hpp
// sighandler_t signal(int signum, sighandler_t handler) noexcept;

int raise(int sig) noexcept;

int sigemptyset(sigset_t *set) noexcept;
int sigfillset(sigset_t *set) noexcept;
int sigaddset(sigset_t *set, int signo) noexcept;
int sigdelset(sigset_t *set, int signo) noexcept;
int sigismember(const sigset_t *set, int signo) noexcept;

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) noexcept;
int sigpending(sigset_t *set) noexcept;
int sigsuspend(const sigset_t *sigmask) noexcept; // Should take const sigset_t *
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) noexcept;

// Other signal functions (kill is often in a different header like unistd.h or sys/types.h)
// int kill(xinim::pid_t pid, int sig) noexcept;

} // extern "C"
