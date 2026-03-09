#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Options {
    bool append = false;
    bool ignore_interrupts = false;
    std::vector<std::string> paths;
};

void print_usage() {
    std::cerr << "Usage: tee [-a] [-i] [file ...]\n";
}

bool parse_arguments(int argc, char* argv[], Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (!arg.empty() && arg[0] == '-' && arg != "-") {
            if (arg == "-a") {
                options.append = true;
            } else if (arg == "-i") {
                options.ignore_interrupts = true;
            } else {
                return false;
            }
        } else {
            options.paths.emplace_back(arg);
        }
    }
    return true;
}

}

int main(int argc, char* argv[]) {
    Options options;
    if (!parse_arguments(argc, argv, options)) {
        print_usage();
        return 1;
    }

    (void)options.ignore_interrupts;

    std::vector<std::ofstream> outputs;
    outputs.reserve(options.paths.size());

    const auto open_mode = options.append ? (std::ios::out | std::ios::app)
                                          : (std::ios::out | std::ios::trunc);
    for (const auto& path : options.paths) {
        outputs.emplace_back(path, open_mode);
        if (!outputs.back()) {
            std::cerr << "tee: cannot open " << path << '\n';
            return 2;
        }
    }

    std::string line;
    bool first = true;
    while (std::getline(std::cin, line)) {
        if (!first) {
            std::cout << '\n';
            for (auto& out : outputs) {
                out << '\n';
            }
        }
        first = false;

        std::cout << line;
        for (auto& out : outputs) {
            out << line;
        }
    }

    if (!std::cin.eof() && std::cin.fail()) {
        return 1;
    }

    std::cout.flush();
    for (auto& out : outputs) {
        out.flush();
    }
    return 0;
}
