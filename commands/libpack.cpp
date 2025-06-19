/**
 * @file libpack.cpp
 * @brief Pack ASCII assembly code using compression tables.
 * @author Andy Tanenbaum (original author)
 * @date 2023-10-28 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the `libpack` utility.
 * It compresses ASCII assembly source code by replacing common patterns
 * with shorter representations using a predefined lookup table.
 * The modern implementation uses efficient string processing, proper
 * I/O handling, and comprehensive error checking.
 *
 * Usage: libpack < input.s > output.packed
 */

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <iterator>

namespace {

/**
 * @brief Compression lookup table for common assembly patterns.
 */
const std::array<std::string_view, 128> compression_table = {
    "push ax", "ret", "mov bp,sp", "push bp", "pop bp", "mov sp,bp", ".text",
    "xor ax,ax", "push 4(bp)", "pop bx", "pop si", "cbw", "movb al,(bx)",
    "pop ax", "xorb ah,ah", "mov ax,#1", "call _callm1", "add sp,#16",
    "mov bx,4(bp)", "push 6(bp)", "mov -2(bp),ax", "I0013:", "call .cppuu",
    "mov ax,-2(bp)", "add 4(bp),#1", "or ax,ax", "jmp I0011", "mov bx,8(bp)",
    "push dx", "mov cx,#2", "mov bx,#2", "I0011:", "I0012:", "push -2(bp)",
    "mov ax,4(bp)", "mov ax,-4(bp)", "add sp,#6", "and ax,#255", "push bx",
    "mov bx,-2(bp)", "loop 2b", "jcxz 1f", ".word 4112", "mov ax,(bx)",
    "mov -4(bp),ax", "jmp I0013", ".data", "mov bx,6(bp)", "mov (bx),ax",
    "je I0012", ".word 8224", ".bss", "mov ax,#2", "call _len", "call _callx",
    ".word 28494", ".word 0", "push -4(bp)", "movb (bx),al", "mov bx,ax",
    "mov -2(bp),#0", "I0016:", ".word 514", ".word 257", "mov ", "push ",
    ".word ", "pop ", "add ", "4(bp)", "-2(bp)", "(bx)", ".define ",
    ".globl ", "movb ", "xor ", "jmp ", "cmp ", "6(bp)", "-4(bp)", "-6(bp)",
    "#16", "#1", "#0", "#2", "#255", "#8", "#4", "ax", "bx", "cx", "dx",
    "sp", "bp", "si", "di", "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
    ",", "(", ")", "[", "]", ":", ";", "+", "-", "*", "/", "%", "&", "|",
    "^", "~", "!", "<", ">", "=", "?", "@", "#", "$", "\\", "'", "\"", "`",
    "\t", "\n", " "
};

/**
 * @brief Compression engine class.
 */
class CompressionEngine {
public:
    CompressionEngine() {
        build_lookup_map();
    }
    
    /**
     * @brief Compress input text using the compression table.
     * @param input Input text to compress.
     * @return Compressed output.
     */
    std::vector<uint8_t> compress(const std::string& input) {
        std::vector<uint8_t> output;
        output.reserve(input.size() / 2); // Estimate compression ratio
        
        size_t pos = 0;
        while (pos < input.size()) {
            bool found_match = false;
            
            // Try to find the longest matching pattern
            for (size_t pattern_len = std::min(input.size() - pos, max_pattern_length_);
                 pattern_len > 0 && !found_match; --pattern_len) {
                
                std::string_view candidate(input.data() + pos, pattern_len);
                auto it = pattern_to_index_.find(candidate);
                
                if (it != pattern_to_index_.end()) {
                    // Found a match - emit the compressed token
                    output.push_back(static_cast<uint8_t>(it->second));
                    pos += pattern_len;
                    found_match = true;
                }
            }
            
            if (!found_match) {
                // No pattern matched - emit literal character
                if (static_cast<uint8_t>(input[pos]) < compression_table.size()) {
                    // Character conflicts with compression token - escape it
                    output.push_back(255); // Escape marker
                    output.push_back(static_cast<uint8_t>(input[pos]));
                } else {
                    output.push_back(static_cast<uint8_t>(input[pos]));
                }
                ++pos;
            }
        }
        
        return output;
    }
    
    /**
     * @brief Decompress data using the compression table.
     * @param compressed Compressed data to decompress.
     * @return Decompressed text.
     */
    std::string decompress(const std::vector<uint8_t>& compressed) {
        std::string output;
        output.reserve(compressed.size() * 2); // Estimate expansion ratio
        
        for (size_t i = 0; i < compressed.size(); ++i) {
            uint8_t token = compressed[i];
            
            if (token == 255 && i + 1 < compressed.size()) {
                // Escaped character
                output += static_cast<char>(compressed[i + 1]);
                ++i; // Skip the escaped character
            } else if (token < compression_table.size()) {
                // Compression token
                output += compression_table[token];
            } else {
                // Literal character
                output += static_cast<char>(token);
            }
        }
        
        return output;
    }

private:
    void build_lookup_map() {
        max_pattern_length_ = 0;
        
        for (size_t i = 0; i < compression_table.size(); ++i) {
            if (!compression_table[i].empty()) {
                pattern_to_index_[compression_table[i]] = i;
                max_pattern_length_ = std::max(max_pattern_length_, compression_table[i].length());
            }
        }
    }
    
    std::unordered_map<std::string_view, size_t> pattern_to_index_;
    size_t max_pattern_length_ = 0;
};

/**
 * @brief Read entire input stream into a string.
 */
std::string read_input() {
    std::string content;
    std::string line;
    
    while (std::getline(std::cin, line)) {
        content += line + '\n';
    }
    
    return content;
}

/**
 * @brief Write binary data to stdout.
 */
void write_output(const std::vector<uint8_t>& data) {
    std::cout.write(reinterpret_cast<const char*>(data.data()), data.size());
}

/**
 * @brief Print compression statistics.
 */
void print_statistics(size_t original_size, size_t compressed_size) {
    if (original_size > 0) {
        double ratio = static_cast<double>(compressed_size) / original_size * 100.0;
        std::cerr << "Original size: " << original_size << " bytes" << std::endl;
        std::cerr << "Compressed size: " << compressed_size << " bytes" << std::endl;
        std::cerr << "Compression ratio: " << std::fixed << std::setprecision(1) 
                  << ratio << "%" << std::endl;
    }
}

} // namespace

/**
 * @brief Main entry point for the libpack command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char* argv[]) {
    try {
        bool verbose = false;
        bool decompress_mode = false;
        
        // Parse command line options
        for (int i = 1; i < argc; ++i) {
            std::string_view arg(argv[i]);
            if (arg == "-v" || arg == "--verbose") {
                verbose = true;
            } else if (arg == "-d" || arg == "--decompress") {
                decompress_mode = true;
            } else if (arg == "-h" || arg == "--help") {
                std::cout << "Usage: libpack [-v] [-d] < input > output" << std::endl;
                std::cout << "  -v, --verbose     Print compression statistics" << std::endl;
                std::cout << "  -d, --decompress  Decompress instead of compress" << std::endl;
                std::cout << "  -h, --help        Show this help message" << std::endl;
                return 0;
            } else {
                std::cerr << "Unknown option: " << arg << std::endl;
                return 1;
            }
        }
        
        CompressionEngine engine;
        
        if (decompress_mode) {
            // Read binary input for decompression
            std::vector<uint8_t> compressed_data;
            char byte;
            while (std::cin.read(&byte, 1)) {
                compressed_data.push_back(static_cast<uint8_t>(byte));
            }
            
            std::string decompressed = engine.decompress(compressed_data);
            std::cout << decompressed;
            
            if (verbose) {
                std::cerr << "Decompressed " << compressed_data.size() 
                          << " bytes to " << decompressed.size() << " bytes" << std::endl;
            }
        } else {
            // Compression mode
            std::string input = read_input();
            auto compressed = engine.compress(input);
            
            write_output(compressed);
            
            if (verbose) {
                print_statistics(input.size(), compressed.size());
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "libpack: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
