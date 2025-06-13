#pragma once

// Common signal number definitions and helper types shared
// between the kernel and user space. These constants
// mirror the historical MINIX signal assignments.
#include "../../include/xinim/core_types.hpp" // For nullptr and other potential core types

inline constexpr int NR_SIGS = 16; /* number of signals used */
inline constexpr int NSIG = 16;    /* number of signals used */

inline constexpr int SIGHUP = 1;   /* hangup */
inline constexpr int SIGINT = 2;   /* interrupt (DEL) */
inline constexpr int SIGQUIT = 3;  /* quit (ASCII FS) */
inline constexpr int SIGILL = 4;   /* illegal instruction (not reset when caught)*/
inline constexpr int SIGTRAP = 5;  /* trace trap (not reset when caught) */
inline constexpr int SIGIOT = 6;   /* IOT instruction */
inline constexpr int SIGEMT = 7;   /* EMT instruction */
inline constexpr int SIGFPE = 8;   /* floating point exception */
inline constexpr int SIGKILL = 9;  /* kill (cannot be caught or ignored) */
inline constexpr int SIGBUS = 10;  /* bus error */
inline constexpr int SIGSEGV = 11; /* segmentation violation */
inline constexpr int SIGSYS = 12;  /* bad argument to system call */
inline constexpr int SIGPIPE = 13; /* write on a pipe with no one to read it */
inline constexpr int SIGALRM = 14; /* alarm clock */
inline constexpr int SIGTERM = 15; /* software termination signal from kill */

inline constexpr int STACK_FAULT = 16; /* used by kernel to signal stack fault */

using sighandler_t = void (*)(int) noexcept;

// The signal() function installs a handler for the given signal number
// and returns the previous handler.
extern "C" sighandler_t signal(int signum, sighandler_t handler) noexcept; // Added noexcept

inline constexpr sighandler_t SIG_DFL = nullptr;
inline constexpr sighandler_t SIG_IGN = reinterpret_cast<sighandler_t>(1);
