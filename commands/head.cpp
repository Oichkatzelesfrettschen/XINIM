/**
 * @file head.cpp
 * @brief Print the first few lines of files.
 * @author Andy Tanenbaum (original author)
 * @date 2023-10-28 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the classic `head` utility.
 * It displays the first N lines of one or more files. The modern implementation
 * uses iostreams for robust file handling, proper command-line parsing,
 * and exception-safe error handling.
 *
 * Usage: head [-n count] [file...]
 *   -n count  Number of lines to display (default: 10)
 */

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

/**
 * @brief Head options structure.
 */
struct HeadOptions {
    /**
     * @brief Number of lines to display from each input.
     */
    size_t line_count = 10;

    /**
     * @brief Collection of files to read; a single dash represents stdin.
     */
    std::vector<std::filesystem::path> files;
};

/**
 * @brief Head engine class for processing files.
 */
class HeadEngine {
  public:
    /**
     * @brief Construct the engine with parsed options.
     * @param opts Parsed options.

     */
    explicit HeadEngine(const HeadOptions &opts) : options_(opts) {}

    /**
     * @brief Process all specified files or stdin.
     * @return Exit status (0 = success, 1 = error).
     */
    int run() {
        if (options_.files.empty()) {
            // Read from stdin
            return process_stream(std::cin, "") ? 0 : 1;
        } else {
            // Process each file
            bool any_errors = false;

            for (size_t i = 0; i < options_.files.size(); ++i) {
                const auto &filepath = options_.files[i];

                // Print header if multiple files
                if (options_.files.size() > 1) {
                    if (i > 0)
                        std::cout << std::endl;
                    std::cout << "==> " << filepath.string() << " <==" << std::endl;
                }

                if (filepath == "-") {
                    if (!process_stream(std::cin, "-")) {
                        any_errors = true;
                    }
                } else {
                    std::ifstream file(filepath);
                    if (!file) {
                        std::cerr << "head: cannot open " << filepath.string() << ": "
                                  << std::strerror(errno) << std::endl;
                        any_errors = true;
                        continue;
                    }

                    if (!process_stream(file, filepath.string())) {
                        any_errors = true;
                    }
                }
            }

            return any_errors ? 1 : 0;
        }
    }

  private:
    // clang-format off
    /**
     * @brief Process a single input stream.
     * @param stream Input stream to process.
     * @param filename Filename for error messages.
     * @return True on success; false on error.
     * @sideeffects Writes lines to @c std::cout.
     * @thread_safety Not thread-safe.
     */
    bool process_stream(std::istream& stream, const std::string& filename) {
        std::string line;
        size_t lines_printed = 0;
        
        while (lines_printed < options_.line_count && std::getline(stream, line)) {
            std::cout << line << std::endl;
            ++lines_printed;
        }
        
        return !stream.bad();
    }
    // clang-format on

    /**
     * @brief Options controlling how input is processed.
     */
    HeadOptions options_;
};

// clang-format off
/**
 * @brief Parse command line arguments.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Parsed options structure.
 * @sideeffects None.
 * @thread_safety Not thread-safe.
 * @compat POSIX
 */
// clang-format on
HeadOptions parse_arguments(int argc, char *argv[]) {
    HeadOptions opts;
    int i = 1;

    // Parse options
    while (i < argc && argv[i][0] == '-') {
        std::string_view arg(argv[i]);

        if (arg == "-") {
            // Special case: "-" means stdin
            break;
        } else if (arg == "--") {
            // End of options
            ++i;
            break;
        } else if (arg.starts_with("-n")) {
            // -n option (number of lines)
            std::string_view count_str;

            if (arg.length() > 2) {
                // -n42 format
                count_str = arg.substr(2);
            } else {
                // -n 42 format
                ++i;
                if (i >= argc) {
                    throw std::runtime_error("Option -n requires an argument");
                }
                count_str = argv[i];
            }

            size_t count;
            auto [ptr, ec] =
                std::from_chars(count_str.data(), count_str.data() + count_str.size(), count);

            if (ec != std::errc() || ptr != count_str.data() + count_str.size()) {
                throw std::runtime_error("Invalid line count: " + std::string(count_str));
            }

            if (count == 0) {
                throw std::runtime_error("Line count must be greater than 0");
            }

            opts.line_count = count;
        } else if (arg.length() > 1 && std::isdigit(arg[1])) {
            // Legacy -42 format
            std::string_view count_str = arg.substr(1);
            size_t count;
            auto [ptr, ec] =
                std::from_chars(count_str.data(), count_str.data() + count_str.size(), count);

            if (ec != std::errc() || ptr != count_str.data() + count_str.size()) {
                throw std::runtime_error("Invalid line count: " + std::string(count_str));
            }

            if (count == 0) {
                throw std::runtime_error("Line count must be greater than 0");
            }

            opts.line_count = count;
        } else {
            throw std::runtime_error("Invalid option: " + std::string(arg));
        }

        ++i;
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
    std::cerr << "Usage: head [-n count] [file...]" << std::endl;
    std::cerr << "  -n count  Number of lines to display (default: 10)" << std::endl;
}

} // namespace

/**
 * @brief Main entry point for the head command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char *argv[]) {
    try {
        HeadOptions options = parse_arguments(argc, argv);
        HeadEngine engine(options);
        return engine.run();

    } catch (const std::exception &e) {
        std::cerr << "head: " << e.what() << std::endl;
        print_usage();
        return 1;
    }
}
