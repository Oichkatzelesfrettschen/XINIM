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
 * @brief Entry point for the basename utility.
 *
 * The program prints the filename component of @p argv[1]. When a second argument
 * is supplied, it is interpreted as a suffix to remove from the filename
 * whenever it is present as a trailing substring.
 *
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Zero on success; nonzero on failure.
 * @exception std::filesystem::filesystem_error Propagates filesystem errors
 * encountered during path processing.
 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        /// Inform the user about the required arguments.
        std::cerr << "Usage: basename string [suffix]" << std::endl;
        return 1;
    }

    try {
        /// Convert the provided path to its filename component.
        std::filesystem::path path{argv[1]};
        std::string base = path.filename().string();

        if (argc == 3) {
            /// Optionally remove a user-specified suffix.
            std::string_view suffix{argv[2]};
            if (base.ends_with(suffix)) {
                /// Erase the matching trailing suffix.
                base.erase(base.size() - suffix.size());
            }
        }

        /// Output the resulting basename.
        std::cout << base << std::endl;

    } catch (const std::filesystem::filesystem_error &e) {
        /// Report filesystem errors to the user.
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
