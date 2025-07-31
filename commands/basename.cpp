/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

// Modernized for C++17

#include <filesystem>
#include <string>
#include <string_view>

/* basename - print the last part of a path:	Author: Blaine Garfolo */

int main(int argc, char *argv[]) {
    // Ensure we have at least one path argument
    if (argc < 2) {
        std_err("Usage: basename string [suffix]  \n");
        return 1;
    }

    // Extract the filename component
    std::filesystem::path path{argv[1]};
    std::string base = path.filename().string();

    // Optionally strip the specified suffix
    if (argc == 3) {
        std::string_view suffix{argv[2]};
        if (base.size() >= suffix.size() &&
            base.compare(base.size() - suffix.size(), suffix.size(), suffix) == 0)
            base.erase(base.size() - suffix.size());
    }

    prints("%s \n", base.c_str());
}
