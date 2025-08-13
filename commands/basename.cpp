/**
 * @file basename.cpp
 * @brief Print the last part of a path.
 * @author Blaine Garfolo
 * @date 2023-10-27
 *
 * Modernized for C++23. This program extracts and prints the filename component of a given path,
 * with an option to remove a specified suffix. It adheres to modern C++ practices, including
 * the use of std::filesystem for path manipulation, exception handling for robustness, and
 *
 * type-safe I/O operations.
 */

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Main entry point for the basename command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 *
 * Usage: basename string [suffix]
 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: basename string [suffix]" << std::endl;
        return 1;
    }

    try {
        std::filesystem::path path{argv[1]};
        std::string base = path.filename().string();

        if (argc == 3) {
            std::string_view suffix{argv[2]};
            if (base.ends_with(suffix)) {
                base.erase(base.size() - suffix.size());
            }
        }

        std::cout << base << std::endl;

    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
