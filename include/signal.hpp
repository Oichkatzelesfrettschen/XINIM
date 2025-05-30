/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#pragma once

// Enumeration of all supported signals.  The numeric values match the
// traditional MINIX signal numbers so existing code can continue to work.
enum class Signal : int {
    SIGHUP = 1, // hangup
    SIGINT,     // interrupt (DEL)
    SIGQUIT,    // quit (ASCII FS)
    SIGILL,     // illegal instruction (not reset when caught)
    SIGTRAP,    // trace trap (not reset when caught)
    SIGIOT,     // IOT instruction
    SIGEMT,     // EMT instruction
    SIGFPE,     // floating point exception
    SIGKILL,    // kill (cannot be caught or ignored)
    SIGBUS,     // bus error
    SIGSEGV,    // segmentation violation
    SIGSYS,     // bad argument to system call
    SIGPIPE,    // write on a pipe with no reader
    SIGALRM,    // alarm clock
    SIGTERM,    // software termination signal from kill
    STACK_FAULT // used by kernel to signal stack fault
};

// Total number of standard signals.
constexpr int NR_SIGS = 16;
constexpr int NSIG = NR_SIGS;

using sighandler_t = void (*)(int);
// Arrange a handler for the given signal.
sighandler_t signal(Signal signum, sighandler_t handler);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
