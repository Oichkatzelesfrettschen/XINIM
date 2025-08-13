/**
 * @file grep.cpp
 * @brief Search files for regular expression patterns.
 * @author Martin C. Atkins (original author)
 * @date 2023-10-28 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 * Original program by Martin C. Atkins, University of York, released into public domain.
 *
 * This program is a C++23 modernization of the classic `grep` utility.
 * It searches files for lines matching a regular expression pattern and
 * outputs matching lines. The modern implementation uses the C++ std::regex
 * library for robust pattern matching and modern I/O streams for file handling.
 *
 * Usage:
 *   grep [-vnse] pattern [file...]
 *
 * Options:
 *   -v  Select non-matching lines
 *   -n  Number output lines
 *   -s  Suppress all output (exit status only)
 *   -e  Treat next argument as pattern (useful for patterns starting with -)
*/

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

/**
 * @brief Grep options structure.
 */
struct GrepOptions {
    bool invert_match = false;    // -v flag
    bool number_lines = false;    // -n flag
    bool suppress_output = false; // -s flag
    bool pattern_follows = false; // -e flag
    std::string pattern;
    std::vector<std::filesystem::path> files;
};

/**
 * @brief Grep engine class for processing files and patterns.
 */
class GrepEngine {
  public:
    explicit GrepEngine(const GrepOptions &opts)
        : options_(opts), regex_(opts.pattern, std::regex_constants::extended) {}

    /**
     * @brief Process all specified files or stdin.
     * @return Exit status (0 = matches found, 1 = no matches, 2 = error).
     */
    int run() {
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
                    std::ifstream file(filepath);
                    if (!file) {
                        std::cerr << "grep: " << filepath << ": " << std::strerror(errno)
                                  << std::endl;
                        any_errors = true;
                        continue;
                    }

                    if (process_stream(file, filepath.string())) {
                        any_matches = true;
                    }
                }
            }
        }

        if (any_errors)
            return 2;
        return any_matches ? 0 : 1;
    }

  private:
    /**
     * @brief Process a single input stream.
     * @param stream Input stream to process.
     * @param filename Filename for display purposes (empty for stdin).
     * @return True if any matches were found.
     */
    bool process_stream(std::istream &stream, const std::string &filename) {
        std::string line;
        size_t line_number = 0;
        bool found_matches = false;
        bool show_filename = !filename.empty() && options_.files.size() > 1;

        while (std::getline(stream, line)) {
            ++line_number;

            bool matches = std::regex_search(line, regex_);
            bool should_print =
                (matches && !options_.invert_match) || (!matches && options_.invert_match);

            if (should_print) {
                found_matches = true;

                if (!options_.suppress_output) {
                    if (show_filename) {
                        std::cout << filename << ":";
                    }

                    if (options_.number_lines) {
                        std::cout << line_number << ":";
                    }

                    std::cout << line << std::endl;
                }
            }
        }

        return found_matches;
    }

    GrepOptions options_;
    std::regex regex_;
};

/**
 * @brief Parse command line arguments.
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
                throw std::runtime_error("Invalid option: -" + std::string(1, arg[j]));
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
 * @brief Print usage information.
 */
void print_usage() {
    std::cerr << "Usage: grep [-vnse] pattern [file...]" << std::endl;
    std::cerr << "  -v  Select non-matching lines" << std::endl;
    std::cerr << "  -n  Number output lines" << std::endl;
    std::cerr << "  -s  Suppress all output (exit status only)" << std::endl;
    std::cerr << "  -e  Treat next argument as pattern" << std::endl;
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
        GrepEngine engine(options);
        return engine.run();

    } catch (const std::regex_error &e) {
        std::cerr << "grep: invalid regular expression: " << e.what() << std::endl;
        return 2;
    } catch (const std::exception &e) {
        std::cerr << "grep: " << e.what() << std::endl;
        print_usage();
        return 2;
    }
}
