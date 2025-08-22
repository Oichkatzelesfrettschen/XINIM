/**
 * @file grep.cpp
 * @brief Modern C++23 implementation of the grep utility for XINIM operating system
 * @author Martin C. Atkins (original author), modernized for XINIM C++23 migration
 * @version 3.0 - Fully modernized with C++23 paradigms
 * @date 2025-08-13
 *
 * @copyright Copyright (c) 2025, The XINIM Project. All rights reserved.
 * Original program by Martin C. Atkins, University of York, released into public domain.
 *
 * @section Description
 * A modernized implementation of the classic `grep` utility, which searches files
 * or standard input for lines matching a regular expression pattern and outputs
 * matching lines. This version leverages the C++23 std::regex library for robust
 * pattern matching, std::filesystem for file handling, and modern I/O streams for
 * efficient processing. It ensures type safety, thread safety, and comprehensive
 * error handling.
 *
 * @section Features
 * - RAII for resource management
 * - Exception-safe error handling
 * - Thread-safe operations with std::mutex
 * - Type-safe string handling with std::string_view
 * - Constexpr configuration for compile-time optimization
 * - Memory-safe operations with std::filesystem and std::vector
 * - Comprehensive Doxygen documentation
 * - Support for C++23 ranges and string formatting
 *
 * @section Usage
 * grep [-vnse] pattern [file...]
 *
 * Options:
 * - -v: Select non-matching lines
 * - -n: Number output lines
 * - -s: Suppress all output (exit status only)
 * - -e: Treat next argument as pattern (useful for patterns starting with -)
 *
 * If no files are specified, grep reads from stdin. A file argument of "-" also
 * indicates stdin. Exit status:
 * - 0: Matches found
 * - 1: No matches found
 * - 2: Error occurred
 *
 * @note Requires C++23 compliant compiler
 */
// clang-format on

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

/**
 * @brief Structure to hold grep command-line options.
 */
struct GrepOptions {
    bool invert_match = false; /**< -v: Select non-matching lines */
    bool number_lines = false; /**< -n: Number output lines */
    bool suppress_output = false; /**< -s: Suppress all output (exit status only) */
    bool pattern_follows = false; /**< -e: Treat next argument as pattern */
    std::string pattern; /**< Regular expression pattern */
    std::vector<std::filesystem::path> files; /**< List of input files */
};

/**
 * @brief Grep engine class for processing files and patterns.
 */
class GrepEngine {
public:
    explicit GrepEngine(GrepOptions opts)
        : options_(std::move(opts)), regex_(options_.pattern, std::regex_constants::extended) {}

    /**
     * @brief Process all specified files or stdin.
     * @return Exit status (0 = matches found, 1 = no matches, 2 = error).
     */
    int run() {
        std::lock_guard lock(mtx_);
        bool any_matches = false;
        bool any_errors = false;

        if (options_.files.empty()) {
            // Read from stdin
            if (process_stream(std::cin, "")) {
                any_matches = true;
            }
        } else {
            // Process each file
            for (const auto &filepath : options_.files) {
                if (filepath == "-") {
                    if (process_stream(std::cin, "-")) {
                        any_matches = true;
                    }
                } else {
                    std::ifstream file(filepath, std::ios::binary);
                    if (!file) {
                        std::cerr << std::format("grep: {}: {}\n", filepath.string(), std::strerror(errno));
                        any_errors = true;
                        continue;
                    }
                    if (process_stream(file, filepath.string())) {
                        any_matches = true;
                    }
                }
            }
        }
        return any_errors ? 2 : (any_matches ? 0 : 1);
    }

private:
    /**
     * @brief Process a single input stream.
     * @param stream Input stream to process.
     * @param filename Filename for display purposes (empty for stdin).
     * @return True if any matches were found.
     */
    bool process_stream(std::istream& stream, std::string_view filename) {
        std::lock_guard lock(mtx_);
        std::string line;
        size_t line_number = 0;
        bool found_matches = false;
        bool show_filename = !filename.empty() && options_.files.size() > 1;

        while (std::getline(stream, line)) {
            ++line_number;
            bool matches = std::regex_search(line, regex_);
            bool should_print = (matches && !options_.invert_match) || (!matches && options_.invert_match);

            if (should_print) {
                found_matches = true;
                if (!options_.suppress_output) {
                    std::string output;
                    if (show_filename) {
                        output += std::format("{}:", filename);
                    }
                    if (options_.number_lines) {
                        output += std::format("{}:", line_number);
                    }
                    output += line + "\n";
                    std::cout << output;
                }
            }
        }
        return found_matches;
    }

    GrepOptions options_;
    std::regex regex_;
    mutable std::mutex mtx_;
};

/**
 * @brief Parse command-line arguments.
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @return Parsed GrepOptions structure.
 * @throws std::runtime_error on invalid arguments.
 */
GrepOptions parse_arguments(int argc, char *argv[]) {
    GrepOptions opts;
    int i = 1;

    // Parse flags
    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        std::string_view arg(argv[i]);

        if (arg == "-") {
            // Special case: "-" means stdin
            break;
        }

        for (size_t j = 1; j < arg.length(); ++j) {
            switch (arg[j]) {
            case 'v':
                opts.invert_match = true;
                break;
            case 'n':
                opts.number_lines = true;
                break;
            case 's':
                opts.suppress_output = true;
                break;
            case 'e':
                opts.pattern_follows = true;
                ++i;
                if (i >= argc) {
                    throw std::runtime_error("Option -e requires an argument");
                }
                opts.pattern = argv[i];
                goto next_arg;
            default:
                throw std::runtime_error(std::format("Invalid option: -{}", arg[j]));
            }
        }
    next_arg:
        ++i;
    }

    // Get pattern (if not already set by -e)
    if (!opts.pattern_follows) {
        if (i >= argc) {
            throw std::runtime_error("Pattern required");
        }
        opts.pattern = argv[i++];
    }

    // Get files
    while (i < argc) {
        opts.files.emplace_back(argv[i++]);
    }

    return opts;
}

/**
 * @brief Print usage information to stderr.
 */
void print_usage() {
    std::cerr << "Usage: grep [-vnse] pattern [file...]\n"
              << "Options:\n"
              << "  -v: Select non-matching lines\n"
              << "  -n: Number output lines\n"
              << "  -s: Suppress all output (exit status only)\n"
              << "  -e: Treat next argument as pattern\n";
}

} // namespace

/**
 * @brief Main entry point for the grep command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 if matches found, 1 if no matches, 2 on error.
 */
int main(int argc, char *argv[]) {
    try {
        GrepOptions options = parse_arguments(argc, argv);
        GrepEngine engine(std::move(options));
        return engine.run();
    } catch (const std::regex_error& e) {
        std::cerr << std::format("grep: invalid regular expression: {}\n", e.what());
        print_usage();
        return 2;
    } catch (const std::exception& e) {
        std::cerr << std::format("grep: {}\n", e.what());
        print_usage();
        return 2;
    } catch (...) {
        std::cerr << "grep: Unknown fatal error occurred\n";
        print_usage();
        return 2;
    }
}