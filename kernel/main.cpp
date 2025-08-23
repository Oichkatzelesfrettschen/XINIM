/**
 * @file main.cpp
 * @brief Minimal kernel entry point used for build validation.
 */

#include "console.hpp"
#include "platform_traits.hpp"

/**
 * @brief Kernel entry function.
 *
 * The implementation currently acts as a placeholder and simply prints
 * a startup banner. Real initialization logic will be added once the
 * supporting subsystems are in place.
 *
 * @pre Executed in early boot context with interrupts disabled.
 * @post Returns only for testing; real kernel would not return.
 * @todo Wire up full subsystem initialization and scheduling start-up.
 */
int main() noexcept {
    Console::printf("XINIM kernel stub\n");
    return 0;
}
