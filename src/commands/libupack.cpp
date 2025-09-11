/**
 * @file libupack.cpp
 * @brief Unpack ASCII assembly code compressed by libpack.
 * @author Original MINIX authors
 * @date 2023-10-28 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the `libupack` utility.
 * It decompresses ASCII assembly source code that was compressed using
 * the libpack utility. The modern implementation uses the same compression
 * table and provides efficient decompression with proper error handling.
 *
 * Usage: libupack < input.packed > output.s
 */

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

/**
 * @brief Decompression lookup table matching libpack's compression table.
 */
const std::array<std::string_view, 128> decompression_table = {"push ax",
                                                               "ret",
                                                               "mov bp,sp",
                                                               "push bp",
                                                               "pop bp",
                                                               "mov sp,bp",
                                                               ".text",
                                                               "xor ax,ax",
                                                               "push 4(bp)",
                                                               "pop bx",
                                                               "pop si",
                                                               "cbw",
                                                               "movb al,(bx)",
                                                               "pop ax",
                                                               "xorb ah,ah",
                                                               "mov ax,#1",
                                                               "call _callm1",
                                                               "add sp,#16",
                                                               "mov bx,4(bp)",
                                                               "push 6(bp)",
                                                               "mov -2(bp),ax",
                                                               "I0013:",
                                                               "call .cppuu",
                                                               "mov ax,-2(bp)",
                                                               "add 4(bp),#1",
                                                               "or ax,ax",
                                                               "jmp I0011",
                                                               "mov bx,8(bp)",
                                                               "push dx",
                                                               "mov cx,#2",
                                                               "mov bx,#2",
                                                               "I0011:",
                                                               "I0012:",
                                                               "push -2(bp)",
                                                               "mov ax,4(bp)",
                                                               "mov ax,-4(bp)",
                                                               "add sp,#6",
                                                               "and ax,#255",
                                                               "push bx",
                                                               "mov bx,-2(bp)",
                                                               "loop 2b",
                                                               "jcxz 1f",
                                                               ".word 4112",
                                                               "mov ax,(bx)",
                                                               "mov -4(bp),ax",
                                                               "jmp I0013",
                                                               ".data",
                                                               "mov bx,6(bp)",
                                                               "mov (bx),ax",
                                                               "je I0012",
                                                               ".word 8224",
                                                               ".bss",
                                                               "mov ax,#2",
                                                               "call _len",
                                                               "call _callx",
                                                               ".word 28494",
                                                               ".word 0",
                                                               "push -4(bp)",
                                                               "movb (bx),al",
                                                               "mov bx,ax",
                                                               "mov -2(bp),#0",
                                                               "I0016:",
                                                               ".word 514",
                                                               ".word 257",
                                                               "mov ",
                                                               "push ",
                                                               ".word ",
                                                               "pop ",
                                                               "add ",
                                                               "4(bp)",
                                                               "-2(bp)",
                                                               "(bx)",
                                                               ".define ",
                                                               ".globl ",
                                                               "movb ",
                                                               "xor ",
                                                               "jmp ",
                                                               "cmp ",
                                                               "6(bp)",
                                                               "-4(bp)",
                                                               "-6(bp)",
                                                               "#16",
                                                               "_callm1",
                                                               "call ",
                                                               "8(bp)",
                                                               "xorb ",
                                                               "and ",
                                                               "sub ",
                                                               "-8(bp)",
                                                               "jne ",
                                                               ".cppuu",
                                                               "#1",
                                                               "#0",
                                                               "#2",
                                                               "#255",
                                                               "#8",
                                                               "#4",
                                                               "ax",
                                                               "bx",
                                                               "cx",
                                                               "dx",
                                                               "sp",
                                                               "bp",
                                                               "si",
                                                               "di",
                                                               "al",
                                                               "bl",
                                                               "cl",
                                                               "dl",
                                                               "ah",
                                                               "bh",
                                                               "ch",
                                                               "dh",
                                                               ",",
                                                               "(",
                                                               ")",
                                                               "[",
                                                               "]",
                                                               ":",
                                                               ";",
                                                               "+",
                                                               "-",
                                                               "*",
                                                               "/",
                                                               "%",
                                                               "&",
                                                               "|",
                                                               "^",
                                                               "~",
                                                               "!",
                                                               "<",
                                                               ">",
                                                               "=",
                                                               "?",
                                                               "@",
                                                               "#",
                                                               "$",
                                                               "\\",
                                                               "'",
                                                               "\"",
                                                               "`",
                                                               "\t",
                                                               "\n",
                                                               " "};

/**
 * @brief Decompression engine class.
 */
class DecompressionEngine {
  public:
    /**
     * @brief Decompress binary data to assembly source code.
     * @param compressed_data Input compressed binary data.
     * @return Decompressed assembly source code.
     */
    std::string decompress(const std::vector<uint8_t> &compressed_data) {
        std::string output;
        output.reserve(compressed_data.size() * 3); // Estimate expansion ratio

        for (size_t i = 0; i < compressed_data.size(); ++i) {
            uint8_t token = compressed_data[i];

            if (token == 255 && i + 1 < compressed_data.size()) {
                // Escaped character - next byte is literal
                output += static_cast<char>(compressed_data[i + 1]);
                ++i; // Skip the escaped character
            } else if (token < decompression_table.size()) {
                // Valid compression token
                output += decompression_table[token];
            } else {
                // Literal character (outside compression token range)
                output += static_cast<char>(token);
            }
        }

        return output;
    }
};

/**
 * @brief Read entire input stream as binary data.
 *
 * This routine consumes all bytes from
 * `stdin` and returns them in a
 * contiguous buffer for later processing.
 *
 * @return Vector
 * containing the raw bytes read from standard input.
 */
std::vector<uint8_t> read_binary_input() {
    std::vector<uint8_t> data;

    // Set binary mode for stdin
    std::cin.sync_with_stdio(false);

    char byte;
    while (std::cin.read(&byte, 1)) {
        data.push_back(static_cast<uint8_t>(byte));
    }

    return data;
}

/**
 * @brief Write text output to stdout.
 *
 * @param text Text string to be emitted to the
 * standard output stream.
 */
void write_text_output(const std::string &text) {
    std::cout << text;
    std::cout.flush();
}

/**
 * @brief Print decompression statistics.
 *
 * @param compressed_size Size of the packed input,
 * in bytes.
 * @param decompressed_size Size of the resulting unpacked output, in bytes.
 */
void print_statistics(size_t compressed_size, size_t decompressed_size) {
    if (compressed_size > 0) {
        double ratio = static_cast<double>(decompressed_size) / compressed_size;
        std::cerr << "Compressed size: " << compressed_size << " bytes" << std::endl;
        std::cerr << "Decompressed size: " << decompressed_size << " bytes" << std::endl;
        std::cerr << "Expansion ratio: " << std::fixed << std::setprecision(1) << ratio << "x"
                  << std::endl;
    }
}

} // namespace

/**
 * @brief Main entry point for the libupack command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char *argv[]) {
    try {
        bool verbose = false;

        // Parse command line options
        for (int i = 1; i < argc; ++i) {
            std::string_view arg(argv[i]);
            if (arg == "-v" || arg == "--verbose") {
                verbose = true;
            } else if (arg == "-h" || arg == "--help") {
                std::cout << "Usage: libupack [-v] < input.packed > output.s" << std::endl;
                std::cout << "  -v, --verbose  Print decompression statistics" << std::endl;
                std::cout << "  -h, --help     Show this help message" << std::endl;
                return 0;
            } else {
                std::cerr << "Unknown option: " << arg << std::endl;
                return 1;
            }
        }

        // Read compressed input data
        auto compressed_data = read_binary_input();

        if (compressed_data.empty()) {
            std::cerr << "libupack: no input data" << std::endl;
            return 1;
        }

        // Decompress the data
        DecompressionEngine engine;
        std::string decompressed_text = engine.decompress(compressed_data);

        // Write decompressed output
        write_text_output(decompressed_text);

        // Print statistics if requested
        if (verbose) {
            print_statistics(compressed_data.size(), decompressed_text.size());
        }

    } catch (const std::exception &e) {
        std::cerr << "libupack: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
