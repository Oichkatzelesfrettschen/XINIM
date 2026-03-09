#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool process_stream(std::istream& input) {
    std::string line;
    while (std::getline(input, line)) {
        std::reverse(line.begin(), line.end());
        std::cout << line << '\n';
    }
    return !input.bad();
}

int process_path(std::string_view path) {
    if (path == "-") {
        return process_stream(std::cin) ? 0 : 1;
    }

    std::ifstream file{std::string(path)};
    if (!file) {
        std::cerr << "rev: cannot open " << path << '\n';
        return 1;
    }
    return process_stream(file) ? 0 : 1;
}

}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        return process_stream(std::cin) ? 0 : 1;
    }

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        if (process_path(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
