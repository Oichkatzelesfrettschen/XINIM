/**
 * @file pwd.cpp
 * @brief Print working directory - C++23 modernized version
 * @details A modern C++23 implementation of the `pwd` utility
 *          using std::filesystem to retrieve and print the current
 *          working directory.
 */

#include <filesystem>    // For std::filesystem::current_path, std::filesystem::path, std::filesystem::filesystem_error
#include <iostream>      // For std::cout, std::cerr
#include <system_error>  // For std::error_code (though not directly used here, often related to filesystem_error)
#include <cstdlib>       // For EXIT_SUCCESS, EXIT_FAILURE
#include <print>         // For std::println (C++23)

// TODO: Consider hybrid logic: if XINIM has specific path retrieval needs.
// For standard POSIX-like behavior, std::filesystem::current_path() is generally sufficient.

/**
 * @brief Main entry point for the pwd command.
 * @param argc The number of command-line arguments (unused).
 * @param argv An array of command-line arguments (unused).
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char* argv[]) {
    // Suppress unused parameter warnings if these are truly not needed.
    (void)argc;
    (void)argv;

    try {
        std::filesystem::path current_path = std::filesystem::current_path();
        // std::filesystem::path::string() returns a std::string, which can be passed to std::println.
        // For narrow character sets, it's fine. For wider/platform-specific, .u8string() or other conversions
        // might be considered if outputting to a specific encoding, but for typical console output, .string() is common.
        std::println(std::cout, "{}", current_path.string());
        return EXIT_SUCCESS;
    } catch (const std::filesystem::filesystem_error& e) {
        // filesystem_error contains detailed information, including error codes and paths.
        std::println(std::cerr, "pwd: error retrieving current directory: {} (code: {})", e.what(), e.code().message());
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        // Catch any other standard exceptions that might occur.
        std::println(std::cerr, "pwd: an unexpected error occurred: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        // Catch-all for any other non-standard exceptions.
        std::println(std::cerr, "pwd: an unknown fatal error occurred.");
        return EXIT_FAILURE;
    }
}
