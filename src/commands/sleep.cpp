#include <charconv>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

namespace {

void print_usage() {
    std::cerr << "Usage: sleep seconds\n";
}

}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage();
        return 1;
    }

    const std::string_view arg(argv[1]);
    unsigned int seconds = 0;
    const auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), seconds);
    if (ec != std::errc{} || ptr != arg.data() + arg.size()) {
        std::cerr << "sleep: bad arg\n";
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    return 0;
}
