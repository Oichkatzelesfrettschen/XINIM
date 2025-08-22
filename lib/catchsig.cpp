#include <csignal>
#include <array>    // Using std::array for a compile-time sized vector table
#include <cstddef>  // For nullptr
#include <functional> // For std::function (optional but good practice)
#include <stdexcept>

#include "../include/lib.hpp"    // Core project definitions
#include "../include/signal.hpp" // NR_SIGS constant

// Using C++11 and later std::signal, so no need for a manual `extern "C"` block
// for the dispatcher trampoline. The signature is compatible.
// However, the user-facing `vectab` is still a C-style array of function pointers.

namespace {

    using Handler = void (*)(int);  ///< Alias for a raw POSIX-style handler.
    
    // A centralized, compile-time sized array to store user-defined handlers.
    // This maintains the original contract while providing C++ safety.
    static std::array<Handler, NR_SIGS> handler_vector_table{};

    /**
     * @brief A centralized dispatcher that acts as a trampoline for all signals.
     * * This function is installed as the primary signal handler for all signals
     * from 1 to NR_SIGS. When a signal is received, it dispatches the call
     * to the user-provided handler stored in `handler_vector_table`.
     * * It correctly respects the default (SIG_DFL) and ignore (SIG_IGN) states
     * of signals. This architecture is a common pattern in robust signal
     * handling libraries to centralize logic and simplify cleanup.
     *
     * @param signum The signal number received by the system.
     */
    void signal_dispatcher(int signum) noexcept {
        // Ensure the signal number is within the valid range
        if (signum > 0 && static_cast<size_t>(signum) <= handler_vector_table.size()) {
            // Adjust for 0-based indexing
            auto handler_func = handler_vector_table[signum - 1];

            // Only call the handler if it's a valid, user-defined function.
            // This preserves SIG_DFL and SIG_IGN behavior.
            if (handler_func != SIG_DFL && handler_func != SIG_IGN) {
                if (handler_func) {
                    handler_func(signum);
                }
            }
        }
    }

} // namespace

// The external API, as required by the C-style linkage.
extern "C" {
    Handler vectab[NR_SIGS]; ///< Vector table storing user handlers, a C-style array for backward compatibility.
}

/**
 * @brief Installs the centralized signal dispatcher for all signals.
 *
 * This function initializes the `handler_vector_table` with the provided
 * `vectab` and then registers a single, unified `signal_dispatcher` as
 * the handler for all signals from 1 to NR_SIGS. This ensures that all
 * signal-related logic flows through a controlled and testable entry point.
 *
 * @note This approach elegantly handles the C-style `vectab` while using
 * C++ to provide a robust, single-entry-point dispatcher.
 *
 * @return Always 0.
 */
extern "C" int begsig() noexcept {
    // Copy the C-style vector table to our C++-style array.
    // This is a direct synthesis of the conflicting codebases.
    for (size_t i = 0; i < NR_SIGS; ++i) {
        handler_vector_table[i] = vectab[i];
    }
    
    // Register the single, unified C++ dispatcher for all signals.
    for (int signum = 1; signum <= NR_SIGS; ++signum) {
        std::signal(signum, signal_dispatcher);
    }

    return 0;
}