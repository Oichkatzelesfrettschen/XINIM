// Pure C++23 implementation of POSIX 'echo' utility
// No libc dependencies - only libc++ (C++ standard library)

#include <algorithm>
#include <format>
#include <iostream>
#include <ranges>
#include <span>
#include <string_view>

// C++23 implementation using modern features
namespace {
    // Parse escape sequences using C++23 features
    [[nodiscard]] constexpr char parse_escape(std::string_view seq) noexcept {
        if (seq.size() < 2 || seq[0] != '\\') return '\0';
        
        switch (seq[1]) {
            case 'a':  return '\a';  // Alert (bell)
            case 'b':  return '\b';  // Backspace
            case 'c':  return '\0';  // Suppress trailing newline (special)
            case 'f':  return '\f';  // Form feed
            case 'n':  return '\n';  // Newline
            case 'r':  return '\r';  // Carriage return
            case 't':  return '\t';  // Horizontal tab
            case 'v':  return '\v';  // Vertical tab
            case '\\': return '\\';  // Backslash
            case '0':  {             // Octal sequence
                // C++23 pattern matching would be ideal here
                if (seq.size() >= 4) {
                    // Parse up to 3 octal digits
                    int value = 0;
                    for (size_t i = 1; i <= 3 && i < seq.size(); ++i) {
                        if (seq[i] >= '0' && seq[i] <= '7') {
                            value = value * 8 + (seq[i] - '0');
                        } else {
                            break;
                        }
                    }
                    return static_cast<char>(value);
                }
                return '\0';
            }
            default:   return seq[1];  // Unknown escape, return literal
        }
    }
    
    // Process string with escape sequences using ranges
    [[nodiscard]] std::string process_escapes(std::string_view str, bool& suppress_newline) {
        std::string result;
        result.reserve(str.size());
        
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '\\' && i + 1 < str.size()) {
                if (str[i + 1] == 'c') {
                    suppress_newline = true;
                    break;  // \c suppresses everything after it
                }
                
                char escaped = parse_escape(str.substr(i, 4));
                if (escaped != '\0') {
                    result += escaped;
                    // Skip the escape sequence
                    if (str[i + 1] == '0') {
                        // Octal can be up to 3 digits
                        size_t j = i + 2;
                        while (j < str.size() && j < i + 4 && 
                               str[j] >= '0' && str[j] <= '7') {
                            ++j;
                        }
                        i = j - 1;
                    } else {
                        ++i;  // Skip the backslash and escaped char
                    }
                } else {
                    result += str[i];
                }
            } else {
                result += str[i];
            }
        }
        
        return result;
    }
}

int main(int argc, char* argv[]) {
    // Create span view of arguments (C++23)
    std::span<char*> args(argv + 1, argc - 1);
    
    bool no_newline = false;
    bool interpret_escapes = false;
    size_t start_index = 0;
    
    // Parse options using ranges (C++23)
    for (const auto& arg : args) {
        std::string_view sv(arg);
        if (sv == "-n") {
            no_newline = true;
            ++start_index;
        } else if (sv == "-e") {
            interpret_escapes = true;
            ++start_index;
        } else if (sv == "-E") {
            interpret_escapes = false;
            ++start_index;
        } else if (sv.starts_with("-")) {
            // Handle combined options like -ne
            for (size_t i = 1; i < sv.size(); ++i) {
                switch (sv[i]) {
                    case 'n': no_newline = true; break;
                    case 'e': interpret_escapes = true; break;
                    case 'E': interpret_escapes = false; break;
                    default: goto done_options;  // Unknown option
                }
            }
            ++start_index;
        } else {
            break;  // First non-option argument
        }
    }
    done_options:
    
    // Process arguments using C++23 ranges
    auto output_args = args.subspan(start_index);
    
    bool suppress_newline = no_newline;
    bool first = true;
    
    for (const auto& arg : output_args) {
        if (!first) {
            std::cout << ' ';
        }
        first = false;
        
        std::string_view str(arg);
        if (interpret_escapes) {
            std::string processed = process_escapes(str, suppress_newline);
            std::cout << processed;
        } else {
            std::cout << str;
        }
    }
    
    if (!suppress_newline) {
        std::cout << '\n';
    }
    
    // Ensure output is flushed (C++23 std::flush)
    std::cout.flush();
    
    return 0;
}