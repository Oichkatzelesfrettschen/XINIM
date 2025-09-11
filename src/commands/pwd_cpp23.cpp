// Pure C++23 implementation of POSIX 'pwd' utility
// Print Working Directory - No libc, only libc++

#include <filesystem>
#include <iostream>
#include <system_error>
#include <span>
#include <string_view>

namespace fs = std::filesystem;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    try {
        // Get current working directory using C++23 filesystem
        auto cwd = fs::current_path();
        
        // Print the absolute path
        std::cout << cwd.string() << '\n';
        
        return 0;
    } catch (const fs::filesystem_error& e) {
        // Handle filesystem errors
        std::cerr << "pwd: " << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        // Handle other errors
        std::cerr << "pwd: " << e.what() << '\n';
        return 1;
    }
}