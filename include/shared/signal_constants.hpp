#pragma once

// Common signal number definitions and helper types shared
// between the kernel and user space. These constants
// mirror the historical MINIX signal assignments.
#include <cstddef> // For nullptr

namespace xinim::signals {

inline constexpr int NR_SIGS = 16; /* number of signals used */

#ifndef NSIG
inline constexpr int NSIG = 16;    /* number of signals used */
#endif

#ifndef SIGHUP
inline constexpr int SIGHUP = 1;   /* hangup */
#endif
#ifndef SIGINT  
inline constexpr int SIGINT = 2;   /* interrupt (DEL) */
#endif
#ifndef SIGQUIT
inline constexpr int SIGQUIT = 3;  /* quit (ASCII FS) */
#endif
#ifndef SIGILL
inline constexpr int SIGILL = 4;   /* illegal instruction (not reset when caught)*/
#endif
#ifndef SIGTRAP
inline constexpr int SIGTRAP = 5;  /* trace trap (not reset when caught) */
#endif
#ifndef SIGIOT
inline constexpr int SIGIOT = 6;   /* IOT instruction */
#endif
#ifndef SIGEMT
inline constexpr int SIGEMT = 7;   /* EMT instruction */
#endif
#ifndef SIGFPE
inline constexpr int SIGFPE = 8;   /* floating point exception */
#endif
#ifndef SIGKILL
inline constexpr int SIGKILL = 9;  /* kill (cannot be caught or ignored) */
#endif
#ifndef SIGBUS
inline constexpr int SIGBUS = 10;  /* bus error */
#endif
#ifndef SIGSEGV
inline constexpr int SIGSEGV = 11; /* segmentation violation */
#endif
#ifndef SIGSYS
inline constexpr int SIGSYS = 12;  /* bad argument to system call */
#endif
#ifndef SIGPIPE
inline constexpr int SIGPIPE = 13; /* write on a pipe with no one to read it */
#endif
#ifndef SIGALRM
inline constexpr int SIGALRM = 14; /* alarm clock */
#endif
#ifndef SIGTERM
inline constexpr int SIGTERM = 15; /* software termination signal from kill */
#endif

inline constexpr int STACK_FAULT = 16; /* used by kernel to signal stack fault */

using sighandler_t = void (*)(int) noexcept;

inline constexpr sighandler_t SIG_DFL = nullptr;
inline const sighandler_t SIG_IGN = reinterpret_cast<sighandler_t>(1);

} // namespace xinim::signals

// The signal() function installs a handler for the given signal number
// and returns the previous handler.
extern "C" xinim::signals::sighandler_t signal(int signum, xinim::signals::sighandler_t handler);
