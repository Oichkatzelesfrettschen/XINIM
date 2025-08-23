/**
 * @file echo.cpp
 * @brief Modern C++23 echo utility for XINIM
 * @author Andy Tanenbaum (original), modernized for XINIM by AI
 *
 * Echo command that outputs its arguments to stdout.
 * Supports -n flag to suppress trailing newline.
 *
 * Fully modernized with C++23 features including:
 * - std::print / std::println for output
 * - std::string_view for argument parsing
 * - Standard library exit codes (EXIT_SUCCESS, EXIT_FAILURE)
 * - Robust error handling with try-catch blocks
 */

#include <iostream>    // For std::cerr, std::ios_base (used for errors)
#include <string_view> // For std::string_view
#include <print>       // For std::print, std::println
#include <cstdlib>     // For EXIT_SUCCESS, EXIT_FAILURE
#include <exception>   // For std::exception

/**
 * @brief Modern echo implementation using std::print and std::println.
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings.
 * @return Exit status (EXIT_SUCCESS for success, EXIT_FAILURE for error).
 */
int main(int argc, char* argv[]) {
    // Optional: Disable synchronization with stdio for potentially faster I/O.
    // It's unlikely to be critical for `echo`, but good practice in general
    // when not mixing C-style I/O with C++ streams.
    // std::ios_base::sync_with_stdio(false);

    // Optional: Make std::cout throw exceptions on failure.
    // This can simplify error checking if you prefer exceptions over checking stream states.
    // std::cout.exceptions(std::ios_base::failbit | std::ios_base::badbit);

    try {
        bool suppress_newline = false;
        int first_arg_idx = 1; // Index of the first argument to print.

        // Check for -n flag (suppress trailing newline).
        // argv[0] is the program name. Arguments start at argv[1].
        if (argc > 1 && std::string_view{argv[1]} == "-n") {
            suppress_newline = true;
            first_arg_idx = 2; // If -n is present, actual arguments start from index 2.
        }

        // Iterate over arguments and print them.
        for (int i = first_arg_idx; i < argc; ++i) {
            // Use std::string_view to avoid copying, though argv elements are char*
            // and std::print will handle them efficiently.
            std::print("{}", std::string_view{argv[i]});

            if (i < argc - 1) { // Add a space if it's not the last argument.
                std::print(" ");
            }
        }

        // Add a newline unless -n was specified.
        // This logic correctly handles various cases:
        //   - "echo" (no arguments, newline not suppressed): prints a single newline.
        //   - "echo -n" (no arguments, newline suppressed): prints nothing.
        //   - "echo foo bar" (arguments, newline not suppressed): prints "foo bar\n".
        //   - "echo -n foo bar" (arguments, newline suppressed): prints "foo bar".
        if (!suppress_newline) {
            std::println(""); // Efficiently prints a newline and flushes.
        }
        // If newline is suppressed and no arguments were printed (e.g. "echo -n"), nothing is printed.
        // If newline is suppressed and arguments were printed, they were printed by std::print without a trailing newline.

        return EXIT_SUCCESS; // Indicate successful execution.

    } catch (const std::ios_base::failure& e) {
        // Specifically catch I/O errors if std::cout.exceptions() was set
        // or other iostream operations failed.
        // Using std::println to std::cerr ensures the error message is flushed and ends with a newline.
        std::println(std::cerr, "echo: I/O error: {}", e.what());
        return EXIT_FAILURE; // Indicate failure due to I/O error.
    } catch (const std::exception& e) {
        // Catch other standard library exceptions (e.g., std::bad_alloc).
        std::println(std::cerr, "echo: Error: {}", e.what());
        return EXIT_FAILURE; // Indicate failure due to a standard exception.
    } catch (...) {
        // Catch any other unexpected errors not covered by previous catch blocks.
        std::println(std::cerr, "echo: An unknown error occurred.");
        return EXIT_FAILURE; // Indicate failure due to an unknown error.
    }
}
