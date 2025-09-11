// Pure C++23 implementation of POSIX 'cat' utility
// No libc dependencies - only libc++ (C++ standard library)

#include <algorithm>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

// C++23 Result type using std::expected
template<typename T>
using Result = std::expected<T, std::error_code>;

namespace {
    struct Options {
        bool number_lines = false;          // -n
        bool number_nonblank = false;       // -b
        bool show_ends = false;              // -E
        bool show_tabs = false;              // -T
        bool show_nonprinting = false;      // -v
        bool squeeze_blank = false;          // -s
        bool unbuffered = false;             // -u
    };
    
    // Parse command line options using C++23 features
    [[nodiscard]] Result<std::pair<Options, std::vector<std::string_view>>> 
    parse_arguments(std::span<char*> args) {
        Options opts;
        std::vector<std::string_view> files;
        
        for (const auto& arg : args) {
            std::string_view sv(arg);
            
            if (sv.starts_with("-") && sv.size() > 1 && sv != "-") {
                // Process options
                for (size_t i = 1; i < sv.size(); ++i) {
                    switch (sv[i]) {
                        case 'n': opts.number_lines = true; break;
                        case 'b': opts.number_nonblank = true; opts.number_lines = false; break;
                        case 'E': opts.show_ends = true; break;
                        case 'T': opts.show_tabs = true; break;
                        case 'v': opts.show_nonprinting = true; break;
                        case 's': opts.squeeze_blank = true; break;
                        case 'u': opts.unbuffered = true; break;
                        case 'e': opts.show_ends = true; opts.show_nonprinting = true; break;
                        case 't': opts.show_tabs = true; opts.show_nonprinting = true; break;
                        case 'A': // Show all
                            opts.show_ends = true;
                            opts.show_tabs = true;
                            opts.show_nonprinting = true;
                            break;
                        default:
                            return std::unexpected(
                                std::make_error_code(std::errc::invalid_argument)
                            );
                    }
                }
            } else {
                files.push_back(sv);
            }
        }
        
        // If no files specified, use stdin (represented by "-")
        if (files.empty()) {
            files.push_back("-");
        }
        
        return std::make_pair(opts, files);
    }
    
    // Convert non-printing characters for display
    [[nodiscard]] std::string make_printable(char c, const Options& opts) {
        if (opts.show_tabs && c == '\t') {
            return "^I";
        }
        
        if (opts.show_nonprinting) {
            if (c == '\n' || c == '\t') {
                return std::string(1, c);  // Keep newlines and tabs (unless -T)
            }
            
            if (c < 32) {  // Control characters
                return std::format("^{}", static_cast<char>(c + 64));
            }
            
            if (c == 127) {  // DEL
                return "^?";
            }
            
            if (static_cast<unsigned char>(c) >= 128) {  // High bit set
                return std::format("M-{}", make_printable(c & 0x7F, opts));
            }
        }
        
        return std::string(1, c);
    }
    
    // Process a single file or stream
    [[nodiscard]] Result<void> process_stream(std::istream& input, 
                                              const Options& opts,
                                              size_t& line_number) {
        std::string line;
        size_t blank_lines = 0;
        
        while (std::getline(input, line)) {
            // Handle squeeze blank lines
            if (opts.squeeze_blank && line.empty()) {
                ++blank_lines;
                if (blank_lines > 1) {
                    continue;  // Skip additional blank lines
                }
            } else {
                blank_lines = 0;
            }
            
            // Number lines if requested
            if (opts.number_lines || (opts.number_nonblank && !line.empty())) {
                std::cout << std::format("{:6} ", ++line_number);
            }
            
            // Process line character by character for special display
            if (opts.show_nonprinting || opts.show_tabs) {
                for (char c : line) {
                    std::cout << make_printable(c, opts);
                }
            } else {
                std::cout << line;
            }
            
            // Show line endings if requested
            if (opts.show_ends) {
                std::cout << '$';
            }
            
            std::cout << '\n';
            
            // Flush if unbuffered
            if (opts.unbuffered) {
                std::cout.flush();
            }
        }
        
        return {};
    }
    
    // Process a file by name
    [[nodiscard]] Result<void> process_file(std::string_view filename,
                                            const Options& opts,
                                            size_t& line_number) {
        if (filename == "-") {
            // Read from stdin
            return process_stream(std::cin, opts, line_number);
        }
        
        // Check if file exists using C++23 filesystem
        fs::path filepath(filename);
        if (!fs::exists(filepath)) {
            return std::unexpected(
                std::make_error_code(std::errc::no_such_file_or_directory)
            );
        }
        
        if (!fs::is_regular_file(filepath) && !fs::is_symlink(filepath)) {
            return std::unexpected(
                std::make_error_code(std::errc::is_a_directory)
            );
        }
        
        // Open and process file
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            return std::unexpected(
                std::make_error_code(std::errc::permission_denied)
            );
        }
        
        return process_stream(file, opts, line_number);
    }
}

int main(int argc, char* argv[]) {
    // Create span of arguments (C++23)
    std::span<char*> args(argv + 1, argc - 1);
    
    // Parse arguments
    auto parse_result = parse_arguments(args);
    if (!parse_result) {
        std::cerr << std::format("cat: {}\n", parse_result.error().message());
        return 1;
    }
    
    auto [opts, files] = parse_result.value();
    
    // Process each file
    size_t line_number = 0;
    bool had_error = false;
    
    for (const auto& filename : files) {
        auto result = process_file(filename, opts, line_number);
        if (!result) {
            std::cerr << std::format("cat: {}: {}\n", 
                                    filename, 
                                    result.error().message());
            had_error = true;
            // Continue processing other files
        }
    }
    
    return had_error ? 1 : 0;
}