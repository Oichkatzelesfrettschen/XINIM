/**
 * @file gres.cpp
 * @brief Global search and replace utility using regular expressions.
 * @author Martin C. Atkins (original author)
 * @date 2023-10-28 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 * Original program by Martin C. Atkins, University of York, released into public domain.
 *
 * This program is a C++23 modernization of the `gres` utility.
 * It performs global search and replace operations on text files using
 * regular expressions. The modern implementation uses the C++ <regex>
 * library for pattern matching and replacement.
 *
 * Usage: gres [-g] search_pattern replacement [file...]
 *   -g  Replace only the first occurrence on each line (default: replace all)
 */

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <regex>
#include <filesystem>
#include <system_error>

namespace {

/**
 * @brief Gres options structure.
 */
struct GresOptions {
    bool global_replace = true;     // Replace all occurrences (default)
    std::string search_pattern;
    std::string replacement;
    std::vector<std::filesystem::path> files;
};

/**
 * @brief Gres engine class for processing files and performing replacements.
 */
class GresEngine {
public:
    explicit GresEngine(const GresOptions& opts) 
        : options_(opts), regex_(opts.search_pattern, std::regex_constants::extended) {}
    
    /**
     * @brief Process all specified files or stdin.
     * @return Exit status (0 = replacements made, 1 = no matches, 2 = error).
     */
    int run() {
        bool any_replacements = false;
        bool any_errors = false;
        
        if (options_.files.empty()) {
            // Read from stdin
            if (process_stream(std::cin)) {
                any_replacements = true;
            }
        } else {
            // Process each file
            for (const auto& filepath : options_.files) {
                if (filepath == "-") {
                    if (process_stream(std::cin)) {
                        any_replacements = true;
                    }
                } else {
                    std::ifstream file(filepath);
                    if (!file) {
                        std::cerr << "gres: " << filepath << ": " 
                                  << std::strerror(errno) << std::endl;
                        any_errors = true;
                        continue;
                    }
                    
                    if (process_stream(file)) {
                        any_replacements = true;
                    }
                }
            }
        }
        
        if (any_errors) return 2;
        return any_replacements ? 0 : 1;
    }

private:
    /**
     * @brief Process a single input stream.
     * @param stream Input stream to process.
     * @return True if any replacements were made.
     */
    bool process_stream(std::istream& stream) {
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
            
            std::cout << result << std::endl;
        }
        
        return made_replacements;
    }
    
    GresOptions options_;
    std::regex regex_;
};

/**
 * @brief Parse command line arguments.
 */
GresOptions parse_arguments(int argc, char* argv[]) {
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
 * @brief Print usage information.
 */
void print_usage() {
    std::cerr << "Usage: gres [-g] search_pattern replacement [file...]" << std::endl;
    std::cerr << "  -g  Replace only the first occurrence on each line" << std::endl;
}

} // namespace

/**
 * @brief Main entry point for the gres command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 if replacements made, 1 if no matches, 2 on error.
 */
int main(int argc, char* argv[]) {
    try {
        GresOptions options = parse_arguments(argc, argv);
        GresEngine engine(options);
        return engine.run();
        
    } catch (const std::regex_error& e) {
        std::cerr << "gres: invalid regular expression: " << e.what() << std::endl;
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "gres: " << e.what() << std::endl;
        print_usage();
        return 2;
    }
}
