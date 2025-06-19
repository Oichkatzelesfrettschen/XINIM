/**
 * @file ln.cpp
 * @brief Create hard links to files.
 * @author Andy Tanenbaum (original author)
 * @date 2023-10-28 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the classic `ln` utility.
 * It creates hard links between files, with support for symbolic links
 * and comprehensive error handling. The modern implementation uses
 * the C++ <filesystem> library for robust path operations.
 *
 * Usage: ln [-s] source [target]
 *   -s  Create symbolic link instead of hard link
 */

#include <iostream>
#include <string>
#include <string_view>
#include <filesystem>
#include <system_error>

// POSIX includes for link operations
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

namespace {

/**
 * @brief Link options structure.
 */
struct LinkOptions {
    bool symbolic = false;
    std::filesystem::path source;
    std::filesystem::path target;
};

/**
 * @brief Parse command line arguments.
 */
LinkOptions parse_arguments(int argc, char* argv[]) {
    LinkOptions opts;
    int i = 1;
    
    // Parse flags
    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        std::string_view arg(argv[i]);
        
        if (arg == "-s") {
            opts.symbolic = true;
        } else if (arg == "--") {
            ++i;
            break;
        } else {
            throw std::runtime_error("Invalid option: " + std::string(arg));
        }
        
        ++i;
    }
    
    // Parse source and target
    if (i >= argc) {
        throw std::runtime_error("Source file required");
    }
    
    opts.source = argv[i++];
    
    if (i < argc) {
        opts.target = argv[i];
    } else {
        // Default target is current directory with source filename
        opts.target = ".";
    }
    
    if (i + 1 < argc) {
        throw std::runtime_error("Too many arguments");
    }
    
    return opts;
}

/**
 * @brief Create the appropriate type of link.
 */
void create_link(const LinkOptions& opts) {
    std::filesystem::path source = opts.source;
    std::filesystem::path target = opts.target;
    
    // Check if source exists (for hard links only)
    if (!opts.symbolic && !std::filesystem::exists(source)) {
        throw std::runtime_error("Source file does not exist: " + source.string());
    }
    
    // Check if source is a directory (hard links to directories not allowed)
    if (!opts.symbolic && std::filesystem::exists(source) && 
        std::filesystem::is_directory(source)) {
        throw std::runtime_error("Cannot create hard link to directory: " + source.string());
    }
    
    // If target is a directory, append source filename
    if (std::filesystem::is_directory(target)) {
        target /= source.filename();
    }
    
    // Check if target already exists
    if (std::filesystem::exists(target)) {
        throw std::runtime_error("Target already exists: " + target.string());
    }
    
    // Create the link
    std::error_code ec;
    if (opts.symbolic) {
        std::filesystem::create_symlink(source, target, ec);
    } else {
        std::filesystem::create_hard_link(source, target, ec);
    }
    
    if (ec) {
        throw std::runtime_error("Failed to create link: " + ec.message());
    }
}

/**
 * @brief Print usage information.
 */
void print_usage() {
    std::cerr << "Usage: ln [-s] source [target]" << std::endl;
    std::cerr << "  -s  Create symbolic link instead of hard link" << std::endl;
}

} // namespace

/**
 * @brief Main entry point for the ln command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char* argv[]) {
    try {
        LinkOptions options = parse_arguments(argc, argv);
        create_link(options);
        
    } catch (const std::exception& e) {
        std::cerr << "ln: " << e.what() << std::endl;
        print_usage();
        return 1;
    }
    
    return 0;
}
