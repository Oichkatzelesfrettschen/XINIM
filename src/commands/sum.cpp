#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t k_block_size = 512;

struct checksum_result {
    std::uint16_t checksum{0};
    std::size_t size{0};
};

checksum_result compute_sum(std::istream& input) {
    checksum_result result{};
    char byte = '\0';
    while (input.get(byte)) {
        const auto rotated = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(result.checksum >> 1U) |
            ((result.checksum & 1U) != 0U ? 0x8000U : 0U));
        result.checksum = static_cast<std::uint16_t>(
            (rotated + static_cast<unsigned char>(byte)) & 0xffffU);
        ++result.size;
    }
    return result;
}

bool emit_sum(std::istream& input, std::string_view label) {
    const checksum_result result = compute_sum(input);
    if (!input.eof() && input.fail()) {
        std::cerr << "sum: read error";
        if (!label.empty()) {
            std::cerr << " on " << label;
        }
        std::cerr << '\n';
        return false;
    }

    const std::size_t blocks = (result.size + k_block_size - 1U) / k_block_size;
    std::cout << result.checksum << ' ' << blocks;
    if (!label.empty()) {
        std::cout << ' ' << label;
    }
    std::cout << '\n';
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    const auto arg_count = argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U;
    std::span<char*> args(argv + 1, arg_count);

    if (args.empty()) {
        return emit_sum(std::cin, {}) ? 0 : 1;
    }

    bool all_ok = true;
    for (char* arg : args) {
        const std::string_view path(arg);
        if (path == "-") {
            all_ok = emit_sum(std::cin, {}) && all_ok;
            continue;
        }

        std::ifstream input(arg, std::ios::binary);
        if (!input) {
            std::cerr << "sum: cannot open " << path << '\n';
            all_ok = false;
            continue;
        }

        all_ok = emit_sum(input, path) && all_ok;
    }

    return all_ok ? 0 : 1;
}
