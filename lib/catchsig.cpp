#include <csignal>

#include "../include/lib.hpp"    // Core project definitions
#include "../include/signal.hpp" // NR_SIGS constant

extern "C" {
using Handler = void (*)(int);  ///< Alias for a raw POSIX-style handler.
extern Handler vectab[NR_SIGS]; ///< Vector table storing user handlers.
}

/**
 * @brief Install trampoline-based signal handlers using C++ lambdas.
 *
 * Each signal supported by the system is routed through a lightweight
 * dispatcher. The dispatcher forwards the signal to the user-provided
 * function stored in #vectab while preserving default and ignored actions.
 *
 * @return Always zero.
 */
extern "C" int begsig() noexcept {
    auto dispatcher = [](int signum) noexcept {
        if (signum >= 1 && signum <= NR_SIGS) {
            if (auto func = vectab[signum - 1]; func && func != SIG_DFL && func != SIG_IGN) {
                func(signum);
            }
        }
    };

    for (int signum = 1; signum <= NR_SIGS; ++signum) {
        std::signal(signum, dispatcher);
    }

    return 0;
}
