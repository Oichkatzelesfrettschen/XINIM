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
 */
int main() noexcept {
    Console::printf("XINIM kernel stub\n");
    return 0;
}
