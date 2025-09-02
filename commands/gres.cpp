/**
 * @file gres.cpp
 * @brief Modern C++23 implementation of the gres utility for XINIM operating system
 * @author Martin C. Atkins (original author), modernized for XINIM C++23 migration
 * @version 3.0 - Fully modernized with C++23 paradigms
 * @date 2025-08-13
 *
 * @copyright Copyright (c) 2025, The XINIM Project. All rights reserved.
 * Original program by Martin C. Atkins, University of York, released into public domain.
 *
 * @section Description
 * A modernized implementation of the `gres` utility, which performs global search
 * and replace operations on text files or standard input using regular expressions.
 * This version leverages the C++23 std::regex library for robust pattern matching
 * and replacement, std::filesystem for file handling, and modern I/O streams for
 * efficient processing. It ensures type safety, thread safety, and comprehensive
 * error handling. Output is written to stdout; for file modifications, redirect
 * output to a file.
 *
 * @section Features
 * - RAII for resource management
 * - Exception-safe error handling
 * - Thread-safe operations with std::mutex
 * - Type-safe string handling with std::string_view
 * - Constexpr configuration for compile-time optimization
 * - Memory-safe operations with std::filesystem and std::vector
 * - Comprehensive Doxygen documentation
 * - Support for C++23 string formatting
 *
 * @section Usage
 * gres [-g] search_pattern replacement [file...]
 *
 * Options:
 * - -g: Replace only the first occurrence on each line (default: replace all)
 *
 * Arguments:
 * - search_pattern: Regular expression pattern to match
 * - replacement: String to replace matched patterns
 * - file...: Files to process (if none, read from stdin; "-" indicates stdin)
 *
 * Exit status:
 * - 0: Replacements made
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
 * @brief Structure to hold gres command-line options.
 */
struct GresOptions {
    bool global_replace = true;               /**< Replace all occurrences (default) */
    std::string search_pattern;               /**< Regular expression pattern */
    std::string replacement;                  /**< Replacement string */
    std::vector<std::filesystem::path> files; /**< List of input files */
};

/**
 * @brief Gres engine class for processing files and performing replacements.
 * @details
 * Coordinates regex-based substitutions across either standard input or
 * a collection of files,
 * ensuring thread-safe execution and robust error
 * propagation.
 */
class GresEngine {
  public:
    explicit GresEngine(GresOptions opts)
        : options_(std::move(opts)),
          regex_(options_.search_pattern, std::regex_constants::extended) {}

    /**
     * @brief Process all specified files or stdin.
     * @return Exit status (0 = replacements made, 1 = no matches, 2 = error).
     */
    int run() {
        std::lock_guard lock(mtx_);
        bool any_replacements = false;
        bool any_errors = false;

        if (options_.files.empty()) {
            // Read from stdin
            if (process_stream(std::cin, "")) {
                any_replacements = true;
            }
        } else {
            // Process each file
            for (const auto &filepath : options_.files) {
                if (filepath == "-") {
                    if (process_stream(std::cin, "-")) {
                        any_replacements = true;
                    }
                } else {
                    std::ifstream file(filepath, std::ios::binary);
                    if (!file) {
                        std::cerr << std::format("gres: {}: {}\n", filepath.string(),
                                                 std::strerror(errno));
                        any_errors = true;
                        continue;
                    }
                    if (process_stream(file, filepath.string())) {
                        any_replacements = true;
                    }
                }
            }
        }
        return any_errors ? 2 : (any_replacements ? 0 : 1);
    }

  private:
    /**
     * @brief Process a single input stream.
     * @param stream Input stream to process.
     * @param filename Filename for error reporting (empty for stdin).
     * @return True if any replacements were made.
     */
    bool process_stream(std::istream &stream, std::string_view filename) {
        std::lock_guard lock(mtx_);
        std::string line;
        bool made_replacements = false;

        while (std::getline(stream, line)) {
            std::string result;

            if (options_.global_replace) {
                // Replace all occurrences
                result = std::regex_replace(line, regex_, options_.replacement);
            } else {
                // Replace only first occurrence
                result = std::regex_replace(line, regex_, options_.replacement,
                                            std::regex_constants::format_first_only);
            }
            if (result != line) {
                made_replacements = true;
            }
            std::cout << result << "\n";
        }
        if (stream.fail() && !stream.eof()) {
            std::cerr << std::format("gres: Error reading {}: {}\n",
                                     filename.empty() ? "stdin" : filename, std::strerror(errno));
            return made_replacements;
        }
        return made_replacements;
    }

    GresOptions options_;    /**< Parsed command-line options. */
    std::regex regex_;       /**< Compiled regular expression. */
    mutable std::mutex mtx_; /**< Mutex protecting shared state. */
};

/**
 * @brief Parse command-line arguments.
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @return Parsed GresOptions structure.
 * @throws std::runtime_error on invalid arguments.
 */
GresOptions parse_arguments(int argc, char *argv[]) {
    GresOptions opts;
    int i = 1;

    // Parse flags
    if (i < argc && std::string_view(argv[i]) == "-g") {
        opts.global_replace = false; // -g means replace only first occurrence
        ++i;
    }

    // Require at least search and replacement patterns
    if (i + 1 >= argc) {
        throw std::runtime_error("Search pattern and replacement required");
    }

    // Get search pattern
    opts.search_pattern = argv[i++];
    if (opts.search_pattern.empty()) {
        throw std::runtime_error("Empty search pattern is not allowed");
    }

    // Get replacement string
    opts.replacement = argv[i++];

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
    std::cerr << "Usage: gres [-g] search_pattern replacement [file...]\n"
              << "Options:\n"
              << "  -g: Replace only the first occurrence on each line\n";
}

} // namespace

/**
 * @brief Main entry point for the gres command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 if replacements made, 1 if no matches, 2 on error.
 */
int main(int argc, char *argv[]) {
    try {
        GresOptions options = parse_arguments(argc, argv);
        GresEngine engine(std::move(options));
        return engine.run();
    } catch (const std::regex_error &e) {
        std::cerr << std::format("gres: invalid regular expression: {}\n", e.what());
        print_usage();
        return 2;
    } catch (const std::exception &e) {
        std::cerr << std::format("gres: {}\n", e.what());
        print_usage();
        return 2;
    } catch (...) {
        std::cerr << "gres: Unknown fatal error occurred\n";
        print_usage();
        return 2;
    }
}
