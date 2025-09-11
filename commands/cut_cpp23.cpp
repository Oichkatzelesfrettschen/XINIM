// Pure C++23 implementation of POSIX 'cut' utility
// Extract sections from lines - No libc, only libc++

#include <algorithm>
#include <charconv>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {
    enum class Mode {
        Bytes,      // -b
        Characters, // -c  
        Fields      // -f
    };
    
    struct Range {
        size_t start;
        size_t end;  // SIZE_MAX means "to end"
        
        [[nodiscard]] bool contains(size_t n) const {
            return n >= start && (end == SIZE_MAX || n <= end);
        }
    };
    
    struct Options {
        Mode mode = Mode::Fields;
        std::vector<Range> ranges;
        char delimiter = '\t';
        bool complement = false;     // -v, --complement
        bool only_delimited = false; // -s
        std::string output_delimiter;
        bool has_output_delimiter = false;
    };
    
    // Parse range specification like "1-3,5,7-"
    [[nodiscard]] std::expected<std::vector<Range>, std::string>
    parse_ranges(std::string_view spec) {
        std::vector<Range> ranges;
        
        // Split by comma using C++23 ranges
        for (auto part : spec | std::views::split(',')) {
            std::string_view range_str(part.begin(), part.end());
            
            if (range_str.empty()) {
                return std::unexpected("empty range specification");
            }
            
            Range r;
            
            // Check for dash
            if (auto dash_pos = range_str.find('-'); dash_pos != std::string_view::npos) {
                if (dash_pos == 0) {
                    // "-N" means "1-N"
                    r.start = 1;
                    if (dash_pos + 1 < range_str.size()) {
                        auto result = std::from_chars(
                            range_str.data() + dash_pos + 1,
                            range_str.data() + range_str.size(),
                            r.end
                        );
                        if (result.ec != std::errc{}) {
                            return std::unexpected(
                                std::format("invalid range: {}", range_str)
                            );
                        }
                    } else {
                        r.end = SIZE_MAX;
                    }
                } else if (dash_pos == range_str.size() - 1) {
                    // "N-" means "N to end"
                    auto result = std::from_chars(
                        range_str.data(),
                        range_str.data() + dash_pos,
                        r.start
                    );
                    if (result.ec != std::errc{}) {
                        return std::unexpected(
                            std::format("invalid range: {}", range_str)
                        );
                    }
                    r.end = SIZE_MAX;
                } else {
                    // "N-M"
                    auto result1 = std::from_chars(
                        range_str.data(),
                        range_str.data() + dash_pos,
                        r.start
                    );
                    auto result2 = std::from_chars(
                        range_str.data() + dash_pos + 1,
                        range_str.data() + range_str.size(),
                        r.end
                    );
                    if (result1.ec != std::errc{} || result2.ec != std::errc{}) {
                        return std::unexpected(
                            std::format("invalid range: {}", range_str)
                        );
                    }
                }
            } else {
                // Single number
                auto result = std::from_chars(
                    range_str.data(),
                    range_str.data() + range_str.size(),
                    r.start
                );
                if (result.ec != std::errc{}) {
                    return std::unexpected(
                        std::format("invalid range: {}", range_str)
                    );
                }
                r.end = r.start;
            }
            
            if (r.start == 0) {
                return std::unexpected("fields and positions are numbered from 1");
            }
            
            ranges.push_back(r);
        }
        
        // Sort and merge overlapping ranges
        std::ranges::sort(ranges, [](const Range& a, const Range& b) {
            return a.start < b.start;
        });
        
        return ranges;
    }
    
    // Check if position is selected
    [[nodiscard]] bool is_selected(size_t pos, const std::vector<Range>& ranges) {
        return std::ranges::any_of(ranges, [pos](const Range& r) {
            return r.contains(pos);
        });
    }
    
    // Process line for byte mode
    [[nodiscard]] std::string process_bytes(std::string_view line, 
                                            const Options& opts) {
        std::string result;
        
        for (size_t i = 0; i < line.size(); ++i) {
            bool selected = is_selected(i + 1, opts.ranges);
            if (selected != opts.complement) {
                result += line[i];
            }
        }
        
        return result;
    }
    
    // Process line for character mode (UTF-8 aware in future)
    [[nodiscard]] std::string process_characters(std::string_view line,
                                                 const Options& opts) {
        // For now, treat as bytes (ASCII)
        // TODO: Add proper UTF-8 support
        return process_bytes(line, opts);
    }
    
    // Process line for field mode
    [[nodiscard]] std::string process_fields(std::string_view line,
                                            const Options& opts) {
        // Check if line contains delimiter
        if (opts.only_delimited && !line.contains(opts.delimiter)) {
            return "";  // Skip lines without delimiter
        }
        
        std::vector<std::string_view> fields;
        
        // Split by delimiter using C++23 ranges
        for (auto field : line | std::views::split(opts.delimiter)) {
            fields.emplace_back(field.begin(), field.end());
        }
        
        // If no delimiter found, treat whole line as field 1
        if (fields.empty()) {
            fields.push_back(line);
        }
        
        // Build result
        std::string result;
        bool first = true;
        
        for (size_t i = 0; i < fields.size(); ++i) {
            bool selected = is_selected(i + 1, opts.ranges);
            if (selected != opts.complement) {
                if (!first) {
                    if (opts.has_output_delimiter) {
                        result += opts.output_delimiter;
                    } else {
                        result += opts.delimiter;
                    }
                }
                result += fields[i];
                first = false;
            }
        }
        
        return result;
    }
    
    // Process a single line
    [[nodiscard]] std::string process_line(std::string_view line,
                                          const Options& opts) {
        switch (opts.mode) {
            case Mode::Bytes:
                return process_bytes(line, opts);
            case Mode::Characters:
                return process_characters(line, opts);
            case Mode::Fields:
                return process_fields(line, opts);
        }
        return "";
    }
    
    // Process input stream
    void process_stream(std::istream& input, const Options& opts) {
        std::string line;
        while (std::getline(input, line)) {
            auto result = process_line(line, opts);
            if (!result.empty() || !opts.only_delimited) {
                std::cout << result << '\n';
            }
        }
    }
}

int main(int argc, char* argv[]) {
    Options opts;
    std::vector<std::string> files;
    
    // Parse arguments using C++23 span
    std::span<char*> args(argv + 1, argc - 1);
    
    for (size_t i = 0; i < args.size(); ++i) {
        std::string_view arg(args[i]);
        
        if (arg == "-b" || arg == "--bytes") {
            if (++i >= args.size()) {
                std::cerr << "cut: option requires an argument -- 'b'\n";
                return 1;
            }
            opts.mode = Mode::Bytes;
            auto ranges = parse_ranges(args[i]);
            if (!ranges) {
                std::cerr << "cut: " << ranges.error() << '\n';
                return 1;
            }
            opts.ranges = ranges.value();
        } else if (arg == "-c" || arg == "--characters") {
            if (++i >= args.size()) {
                std::cerr << "cut: option requires an argument -- 'c'\n";
                return 1;
            }
            opts.mode = Mode::Characters;
            auto ranges = parse_ranges(args[i]);
            if (!ranges) {
                std::cerr << "cut: " << ranges.error() << '\n';
                return 1;
            }
            opts.ranges = ranges.value();
        } else if (arg == "-f" || arg == "--fields") {
            if (++i >= args.size()) {
                std::cerr << "cut: option requires an argument -- 'f'\n";
                return 1;
            }
            opts.mode = Mode::Fields;
            auto ranges = parse_ranges(args[i]);
            if (!ranges) {
                std::cerr << "cut: " << ranges.error() << '\n';
                return 1;
            }
            opts.ranges = ranges.value();
        } else if (arg == "-d" || arg == "--delimiter") {
            if (++i >= args.size()) {
                std::cerr << "cut: option requires an argument -- 'd'\n";
                return 1;
            }
            std::string_view delim(args[i]);
            if (delim.empty()) {
                std::cerr << "cut: the delimiter must be a single character\n";
                return 1;
            }
            opts.delimiter = delim[0];
        } else if (arg == "-s" || arg == "--only-delimited") {
            opts.only_delimited = true;
        } else if (arg == "--complement") {
            opts.complement = true;
        } else if (arg == "--output-delimiter") {
            if (++i >= args.size()) {
                std::cerr << "cut: option requires an argument\n";
                return 1;
            }
            opts.output_delimiter = args[i];
            opts.has_output_delimiter = true;
        } else if (arg == "--help") {
            std::cout << "Usage: cut OPTION... [FILE]...\n";
            std::cout << "Print selected parts of lines from each FILE.\n\n";
            std::cout << "  -b LIST     select only these bytes\n";
            std::cout << "  -c LIST     select only these characters\n";
            std::cout << "  -f LIST     select only these fields\n";
            std::cout << "  -d DELIM    use DELIM instead of TAB\n";
            std::cout << "  -s          only print lines containing delimiter\n";
            std::cout << "  --complement complement the selection\n";
            std::cout << "  --output-delimiter=STRING  use STRING as output delimiter\n";
            return 0;
        } else if (arg.starts_with("-")) {
            std::cerr << std::format("cut: invalid option '{}'\n", arg);
            return 1;
        } else {
            files.emplace_back(arg);
        }
    }
    
    // Check if ranges were specified
    if (opts.ranges.empty()) {
        std::cerr << "cut: you must specify a list of bytes, characters, or fields\n";
        return 1;
    }
    
    // Process files or stdin
    if (files.empty()) {
        process_stream(std::cin, opts);
    } else {
        for (const auto& filename : files) {
            if (filename == "-") {
                process_stream(std::cin, opts);
            } else {
                std::ifstream file(filename);
                if (!file) {
                    std::cerr << std::format("cut: {}: No such file or directory\n",
                                           filename);
                    return 1;
                }
                process_stream(file, opts);
            }
        }
    }
    
    return 0;
}