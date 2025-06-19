/**
 * @file chmod.cpp
 * @brief Change mode of files.
 * @author Patrick van Kleef (original author)
 * @date 2023-10-27 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the original `chmod` utility from MINIX.
 * It changes the file mode (permissions) of the specified files.
 * It uses modern C++ features like <filesystem> for file operations,
 * robust argument parsing, and structured error reporting.
 *
 * Usage: chmod [mode] file...
 */

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <system_error>
#include <charconv>

namespace {

/**
 * @brief Prints the usage message to standard error.
 */
void printUsage() {
    std::cerr << "Usage: chmod [mode] file..." << std::endl;
}

/**
 * @brief Converts an octal string representation to an integer.
 * @param s The string_view containing the octal number.
 * @return The integer value.
 * @throws std::invalid_argument if the string is not a valid octal number.
 */
int octalStringToInt(std::string_view s) {
    int value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value, 8);
    if (ec != std::errc() || ptr != s.data() + s.size()) {
        throw std::invalid_argument("Invalid octal mode.");
    }
    return value;
}

} // namespace

/**
 * @brief Main entry point for the chmod command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    int new_mode_val;
    try {
        new_mode_val = octalStringToInt(argv[1]);
    } catch (const std::invalid_argument& e) {
        std::cerr << "chmod: " << e.what() << std::endl;
        printUsage();
        return 1;
    }

    auto new_perms = static_cast<std::filesystem::perms>(new_mode_val);
    int status = 0;

    for (int i = 2; i < argc; ++i) {
        std::filesystem::path file_path(argv[i]);
        std::error_code ec;

        std::filesystem::permissions(file_path, new_perms, ec);

        if (ec) {
            std::cerr << "chmod: cannot change permissions of '" << file_path.string() << "': " << ec.message() << std::endl;
            status = 1;
        }
    }

    return status;
}
