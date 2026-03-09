#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct options {
    bool parents{false};
    std::vector<fs::path> paths{};
};

void print_usage() {
    std::cerr << "usage: rmdir [-p] directory ...\n";
}

bool remove_one(const fs::path& path) {
    std::error_code ec;
    const bool removed = fs::remove(path, ec);
    if (!removed || ec) {
        std::cerr << "rmdir: " << path.string() << ": "
                  << (ec ? ec.message() : "directory not removed") << '\n';
        return false;
    }
    return true;
}

bool remove_with_parents(fs::path path) {
    while (!path.empty()) {
        if (!remove_one(path)) {
            return false;
        }
        path = path.parent_path();
        if (path.empty() || path == "." || path == "/") {
            break;
        }
    }
    return true;
}

bool parse_args(std::span<char*> argv, options& parsed) {
    for (const char* raw_arg : argv) {
        const std::string_view arg(raw_arg);
        if (arg == "-p") {
            parsed.parents = true;
            continue;
        }
        if (arg == "--help") {
            print_usage();
            return false;
        }
        if (!arg.empty() && arg.front() == '-') {
            std::cerr << "rmdir: unsupported option: " << arg << '\n';
            print_usage();
            return false;
        }
        parsed.paths.emplace_back(raw_arg);
    }

    if (parsed.paths.empty()) {
        print_usage();
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    const auto arg_count = argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U;
    std::span<char*> args(argv + 1, arg_count);

    options parsed{};
    if (!parse_args(args, parsed)) {
        return parsed.paths.empty() ? 1 : 0;
    }

    bool all_ok = true;
    for (const fs::path& path : parsed.paths) {
        const bool ok = parsed.parents ? remove_with_parents(path) : remove_one(path);
        all_ok = all_ok && ok;
    }

    return all_ok ? 0 : 1;
}
