/**
 * @file r.cpp
 * @brief Utility to dump memory contents from a core file.
 *
 * This modernized implementation replaces raw POSIX system calls with
 * C++23 RAII wrappers, ensuring automatic resource cleanup and improved
 * error handling. The program accepts an optional path to a core file and
 * interactively displays 16-byte aligned memory regions in hexadecimal.
 */

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string_view>

namespace {

/** Last character read by ::hexin(). */
char g_last_char{};

/** Most recently requested address. */
long g_address{};

/** Value read from core file for printing. */
std::uint16_t g_word{};

[[nodiscard]] std::optional<long> hexin();
void hex(std::uint16_t value);
void wrl(std::ifstream &file, long address);

} // namespace

/**
 * @brief Program entry point.
 *
 * Opens the specified core file (defaulting to "core.88") and repeatedly
 * reads hexadecimal addresses from standard input, printing the contents of
 * the corresponding memory locations.
 */
int main(int argc, char *argv[]) {
    const std::string_view path = argc == 2 ? argv[1] : std::string_view{"core.88"};
    std::ifstream core_file(path.data(), std::ios::binary);
    if (!core_file.is_open()) {
        std::perror("can't open core file");
        return EXIT_FAILURE;
    }

    while (true) {
        const auto addr_opt = hexin();
        if (!addr_opt) {
            break; // EOF encountered
        }
        long addr = *addr_opt & ~0xFL; // align to 16-byte boundary
        g_address = addr;              // remember last address
        int count = 0;
        if (g_last_char == ',') {
            const auto ct = hexin();
            count = static_cast<int>(ct.value_or(0));
        }
        do {
            wrl(core_file, addr);
            addr += 16;
        } while (--count > 0);
    }
    return EXIT_SUCCESS;
}

namespace {

/**
 * @brief Read a hexadecimal number from standard input.
 *
 * @return Parsed value or std::nullopt on end-of-file before any digits were
 *         read. The last non-hexadecimal character read is stored in
 *         ::g_last_char.
 */
[[nodiscard]] std::optional<long> hexin() {
    long value = 0;
    int digits = 0;
    while (true) {
        const int ch = std::getchar();
        if (ch == EOF) {
            if (digits == 0) {
                return std::nullopt;
            }
            g_last_char = static_cast<char>(ch);
            return value;
        }
        g_last_char = static_cast<char>(ch);
        if (digits == 0 && g_last_char == '\n') {
            return g_address;
        }
        ++digits;
        if (g_last_char >= '0' && g_last_char <= '9') {
            value = (value << 4) + (g_last_char - '0');
            continue;
        }
        if (g_last_char >= 'a' && g_last_char <= 'f') {
            value = (value << 4) + (10 + g_last_char - 'a');
            continue;
        }
        if (g_last_char >= 'A' && g_last_char <= 'F') {
            value = (value << 4) + (10 + g_last_char - 'A');
            continue;
        }
        return value;
    }
}

/**
 * @brief Print a 16-bit word in hexadecimal.
 * @param value Word to print.
 */
void hex(std::uint16_t value) {
    for (int i = 0; i < 4; ++i) {
        const int k = (value >> (12 - 4 * i)) & 0xF;
        const char c = k <= 9 ? static_cast<char>('0' + k) : static_cast<char>('A' + (k - 10));
        std::printf("%c", c);
    }
    std::printf(" ");
}

/**
 * @brief Write a line of memory contents starting at a given address.
 *
 * @param file   Open core file stream.
 * @param address Memory address to read.
 */
void wrl(std::ifstream &file, long address) {
    std::printf("%5lx:  ", address);
    file.seekg(address, std::ios::beg);
    if (!file) {
        throw std::system_error(errno, std::system_category(), "Can't seek");
    }
    for (int i = 0; i < 8; ++i) {
        file.read(reinterpret_cast<char *>(&g_word), sizeof(g_word));
        if (!file) {
            throw std::system_error(errno, std::system_category(), "Read error");
        }
        hex(g_word);
    }
    std::printf("\n");
}

} // namespace
