#include <iostream>
#include <filesystem>
#include <chrono>
#include <vector>
#include <string_view>
#include <system_error>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>

namespace fs = std::filesystem;

void print_usage_and_exit() {
    std::cerr << "Usage: touch [-c] file...\n";
    std::exit(1);
}

void print_error(const std::string_view msg, const std::string_view file = {}) {
    std::cerr << "touch: " << msg;
    if (!file.empty()) std::cerr << " '" << file << "'";
    std::cerr << '\n';
}

bool touch_file(const std::string_view path, bool no_create) {
    // Try to update timestamps if file exists
    if (fs::exists(path)) {
        struct stat st{};
        if (::stat(std::string(path).c_str(), &st) != 0) {
            print_error("cannot stat", path);
            return false;
        }
        if (!S_ISREG(st.st_mode)) {
            print_error("not a regular file", path);
            return false;
        }
        // Set both access and modification times to now
        struct utimbuf new_times;
        new_times.actime = new_times.modtime = std::time(nullptr);
        if (::utime(std::string(path).c_str(), &new_times) != 0) {
            print_error(std::strerror(errno), path);
            return false;
        }
        return true;
    } else {
        // File does not exist
        if (no_create) return true;
        int fd = ::creat(std::string(path).c_str(), 0666);
        if (fd < 0) {
            print_error(std::strerror(errno), path);
            return false;
        }
        ::close(fd);
        return true;
    }
}

int main(int argc, char* argv[]) {
    bool no_create = false;
    std::vector<std::string_view> files;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg.starts_with('-') && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case 'c': no_create = true; break;
                    case 'f': /* ignore for compatibility */ break;
                    default: print_usage_and_exit();
                }
            }
        } else {
            files.push_back(arg);
        }
    }

    if (files.empty()) print_usage_and_exit();

    int exit_code = 0;
    for (const auto& file : files) {
        if (!touch_file(file, no_create)) {
            exit_code = 1;
        }
    }
    return exit_code;
}
