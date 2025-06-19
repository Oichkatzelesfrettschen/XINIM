/**
 * @file chown.cpp
 * @brief Change owner of files.
 * @author Patrick van Kleef (original author)
 * @date 2023-10-27 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the original `chown` utility from MINIX.
 * It changes the owner of the specified files to a new owner, keeping the group the same.
 * It uses modern C++ features for argument parsing and error reporting, while still
 * relying on POSIX functions for user and file management.
 *
 * Usage: chown username file...
 */

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <system_error>
#include <cerrno>

// POSIX includes
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>

namespace {

/**
 * @brief Prints the usage message to standard error.
 */
void printUsage() {
    std::cerr << "Usage: chown username file..." << std::endl;
}

} // namespace

/**
 * @brief Main entry point for the chown command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::string username(argv[1]);
    struct passwd* pwd = getpwnam(username.c_str());

    if (pwd == nullptr) {
        std::cerr << "chown: unknown user: " << username << std::endl;
        return 1;
    }

    uid_t new_owner_uid = pwd->pw_uid;
    int status = 0;

    for (int i = 2; i < argc; ++i) {
        std::filesystem::path file_path(argv[i]);
        struct stat st;

        if (stat(file_path.c_str(), &st) != 0) {
            std::cerr << "chown: cannot access '" << file_path.string() << "': " << strerror(errno) << std::endl;
            status = 1;
            continue;
        }

        if (chown(file_path.c_str(), new_owner_uid, st.st_gid) != 0) {
            std::cerr << "chown: changing ownership of '" << file_path.string() << "': " << strerror(errno) << std::endl;
            status = 1;
        }
    }

    return status;
}
