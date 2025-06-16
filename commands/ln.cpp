/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* ln - link a file		Author: Andy Tanenbaum */

#include "stat.hpp"
#include <filesystem>
#include <string>

// No global buffers needed; std::filesystem manages paths safely.
// The path objects automatically free any allocated resources
// when they go out of scope (RAII).

int main(int argc, char **argv) {
    // Validate argument count
    if (argc < 2 || argc > 3)
        return usage();

    std::filesystem::path src{argv[1]};

    // Ensure the source exists
    if (access(src.c_str(), 0) < 0) {
        std_err("ln: cannot access ");
        std_err(argv[1]);
        std_err("\n");
        return 1;
    }

    // Source must not be a directory
    struct stat sb{};
    if (stat(src.c_str(), &sb) >= 0 && (sb.st_mode & S_IFMT) == S_IFDIR)
        return usage();

    std::filesystem::path dest =
        (argc == 2) ? std::filesystem::path{"."} : std::filesystem::path{argv[2]};

    // If destination is a directory append the source file name
    if (stat(dest.c_str(), &sb) >= 0 && (sb.st_mode & S_IFMT) == S_IFDIR)
        dest /= src.filename();

    // Create the link
    if (link(src.c_str(), dest.c_str()) != 0) {
        std_err("ln: Can't link\n");
        return 1;
    }

    return 0;
}

static int usage() {
    std_err("Usage: ln file1 [file2]\n");
    return 1;
}
