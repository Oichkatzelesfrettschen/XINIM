// Pure C++23 implementation of POSIX 'env' utility
// Environment variable management - No libc, only libc++

#include <algorithm>
#include <cstdlib>
#include <expected>
#include <format>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <map>

// Platform-specific environment access
extern "C" char** environ;

namespace {
    struct Options {
        bool ignore_environment = false;  // -i
        bool null_terminated = false;     // -0
        bool debug = false;               // -v (verbose)
        std::map<std::string, std::string> vars;
        std::vector<std::string> command;
    };
    
    [[nodiscard]] std::expected<Options, std::string> 
    parse_arguments(std::span<char*> args) {
        Options opts;
        bool parsing_options = true;
        
        for (size_t i = 0; i < args.size(); ++i) {
            std::string_view arg(args[i]);
            
            if (parsing_options && arg.starts_with("-")) {
                if (arg == "-i" || arg == "--ignore-environment") {
                    opts.ignore_environment = true;
                } else if (arg == "-0" || arg == "--null") {
                    opts.null_terminated = true;
                } else if (arg == "-v" || arg == "--debug") {
                    opts.debug = true;
                } else if (arg == "--") {
                    parsing_options = false;
                } else if (arg == "-") {
                    // Treat as command
                    parsing_options = false;
                    opts.command.emplace_back(arg);
                } else {
                    return std::unexpected(
                        std::format("env: invalid option: {}", arg)
                    );
                }
            } else if (arg.contains('=')) {
                // Variable assignment
                auto pos = arg.find('=');
                opts.vars[std::string(arg.substr(0, pos))] = 
                    std::string(arg.substr(pos + 1));
            } else {
                // Start of command
                parsing_options = false;
                for (size_t j = i; j < args.size(); ++j) {
                    opts.command.emplace_back(args[j]);
                }
                break;
            }
        }
        
        return opts;
    }
    
    // Get environment as C++23 map
    [[nodiscard]] std::map<std::string, std::string> get_environment() {
        std::map<std::string, std::string> env;
        
        if (environ) {
            for (char** p = environ; *p; ++p) {
                std::string_view entry(*p);
                if (auto pos = entry.find('='); pos != std::string_view::npos) {
                    env[std::string(entry.substr(0, pos))] = 
                        std::string(entry.substr(pos + 1));
                }
            }
        }
        
        return env;
    }
    
    // Set environment variable using C++23
    void set_env(const std::string& name, const std::string& value) {
        // Using C++23 string formatting for safety
        auto formatted = std::format("{}={}", name, value);
        
        // Platform-specific setenv (we'll use putenv as fallback)
        #ifdef _WIN32
            _putenv(formatted.c_str());
        #else
            setenv(name.c_str(), value.c_str(), 1);
        #endif
    }
    
    // Execute command with modified environment
    [[nodiscard]] int execute_command(const std::vector<std::string>& cmd) {
        if (cmd.empty()) {
            return 0;
        }
        
        // Build command string
        std::string command = cmd[0];
        for (size_t i = 1; i < cmd.size(); ++i) {
            command += ' ';
            // Quote arguments with spaces
            if (cmd[i].contains(' ')) {
                command += std::format("\"{}\"", cmd[i]);
            } else {
                command += cmd[i];
            }
        }
        
        // Execute using system (C++23 std::system)
        return std::system(command.c_str());
    }
    
    // Print environment using C++23 ranges
    void print_environment(const std::map<std::string, std::string>& env,
                          bool null_terminated) {
        // Use C++23 ranges to process and print
        auto sorted_view = env | std::views::all;
        
        for (const auto& [key, value] : sorted_view) {
            std::cout << std::format("{}={}", key, value);
            if (null_terminated) {
                std::cout << '\0';
            } else {
                std::cout << '\n';
            }
        }
        
        if (null_terminated) {
            std::cout.flush();
        }
    }
}

int main(int argc, char* argv[]) {
    // Parse arguments using C++23 span
    std::span<char*> args(argv + 1, argc - 1);
    
    auto parse_result = parse_arguments(args);
    if (!parse_result) {
        std::cerr << parse_result.error() << '\n';
        return 1;
    }
    
    auto opts = parse_result.value();
    
    // Get current environment
    auto env = opts.ignore_environment 
        ? std::map<std::string, std::string>{} 
        : get_environment();
    
    // Apply variable modifications
    for (const auto& [name, value] : opts.vars) {
        env[name] = value;
        if (opts.debug) {
            std::cerr << std::format("env: setting {}={}\n", name, value);
        }
    }
    
    // Set environment variables
    for (const auto& [name, value] : env) {
        set_env(name, value);
    }
    
    // Execute command or print environment
    if (opts.command.empty()) {
        print_environment(env, opts.null_terminated);
        return 0;
    } else {
        if (opts.debug) {
            std::cerr << std::format("env: executing: {}\n", 
                                    opts.command[0]);
        }
        return execute_command(opts.command);
    }
}