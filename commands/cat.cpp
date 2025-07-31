/**
 * @file cat.cpp
 * @brief Concatenate files to standard output - C++23 modernized version
 * @author Andy Tanenbaum (original), Modernized for C++23
 *
 * Modern C++23 implementation using RAII, C++ streams, and structured error handling.
 */

#include "blocksiz.hpp" // For BLOCK_SIZE

#include <array>       // For std::array
#include <cstdlib>     // For EXIT_SUCCESS, EXIT_FAILURE
#include <cstring>     // For strerror
#include <cerrno>      // For errno
#include <fstream>     // For std::ifstream
#include <iostream>    // For std::cin, std::cout, std::cerr, std::ios_base
#include <print>       // For std::println (C++23)
#include <string_view> // For std::string_view
#include <vector>      // For std::vector

// Forward declaration for the function that processes content from input to output stream
static void stream_content(std::istream& in, std::ostream& out);

int main(int argc, char* argv[]) {
    // Set exception masks for standard streams to throw exceptions on errors.
    // This helps in centralizing error handling.
    try {
        std::cin.exceptions(std::ios_base::badbit | std::ios_base::failbit);
        std::cout.exceptions(std::ios_base::badbit | std::ios_base::failbit);
        std::cerr.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    } catch (const std::ios_base::failure& e) {
        // This is unlikely to happen here, but as a precaution:
        std::println(std::cerr, "Error setting up standard stream exceptions: {}", e.what());
        return EXIT_FAILURE;
    }


    std::vector<std::string_view> args(argv + 1, argv + argc);
    int exit_status = EXIT_SUCCESS;

    if (args.empty()) {
        // No file arguments given, process standard input.
        try {
            stream_content(std::cin, std::cout);
        } catch (const std::ios_base::failure& e) {
            std::println(std::cerr, "Error processing standard input: {}", e.what());
            return EXIT_FAILURE; // Critical error if stdin fails.
        }
    } else {
        // Process each argument. It could be a filename or "-" for stdin.
        for (const auto& arg : args) {
            if (arg == "-") {
                // Argument is "-", process standard input.
                try {
                    // Ensure cin is clear of any previous errors if used multiple times
                    std::cin.clear();
                    stream_content(std::cin, std::cout);
                } catch (const std::ios_base::failure& e) {
                    std::println(std::cerr, "Error processing standard input ('-'): {}", e.what());
                    exit_status = EXIT_FAILURE; // Mark failure, but continue with other files.
                }
            } else {
                // Argument is a filename.
                std::ifstream file(arg.data(), std::ios_base::in | std::ios_base::binary);
                if (!file.is_open()) {
                    std::println(std::cerr, "cat: cannot open {}: {}", arg, strerror(errno));
                    exit_status = EXIT_FAILURE; // Mark failure, but continue.
                    continue; // Move to the next file.
                }

                try {
                    // Enable exceptions for the file stream.
                    file.exceptions(std::ios_base::badbit | std::ios_base::failbit);
                    stream_content(file, std::cout);
                } catch (const std::ios_base::failure& e) {
                    std::println(std::cerr, "Error processing file {}: {}", arg, e.what());
                    exit_status = EXIT_FAILURE; // Mark failure, but file stream closes automatically.
                }
                // file is closed automatically when it goes out of scope (RAII).
            }
        }
    }

    // std::cout is typically flushed on normal program termination.
    // An explicit flush (std::cout.flush()) can be used if immediate output is critical before exit.
    return exit_status;
}

/**
 * @brief Reads from an input stream and writes to an output stream.
 *
 * Uses a buffer to efficiently transfer data. Throws std::ios_base::failure
 * on read/write errors if the streams are configured to do so.
 *
 * @param in The input stream to read from.
 * @param out The output stream to write to.
 */
static void stream_content(std::istream& in, std::ostream& out) {
    // Use BLOCK_SIZE from blocksiz.hpp as the buffer size.
    // If blocksiz.hpp were not available, a fallback like 4096 could be used.
    constexpr std::size_t BUFFER_SIZE = BLOCK_SIZE;
    std::array<char, BUFFER_SIZE> buffer;

    // Loop as long as read operations succeed or some characters are read.
    while (in.read(buffer.data(), buffer.size()) || in.gcount() > 0) {
        // in.gcount() returns the number of characters extracted by the last unformatted input operation.
        // This is crucial for handling the end of the file, where read might get less than BUFFER_SIZE.
        out.write(buffer.data(), in.gcount());
    }
    // If stream exceptions are enabled (as they are in main),
    // any read/write error during this loop will throw std::ios_base::failure.
}
