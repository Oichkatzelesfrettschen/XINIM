/**
 * @file mined_library.cpp
 * @brief MINED Editor Library - without main function for integration
 * @author XINIM Project
 * @version 3.0
 * @date 2025
 */

#include "mined_final.hpp"
#include <algorithm>
#include <execution>
#include <sstream>
#include <cctype>

namespace xinim::mined {

// =============================================================================
// UnicodeText Implementation
// =============================================================================

UnicodeText::UnicodeText(std::string_view str, Encoding enc) 
    : data_(str), encoding_(enc) {
    invalidate_cache();
}

UnicodeText::UnicodeText(const char* str) 
    : UnicodeText(std::string_view(str)) {}

UnicodeText::UnicodeText(char32_t codepoint) {
    append(codepoint);
}

auto UnicodeText::length() const noexcept -> std::size_t {
    if (!char_count_.has_value()) {
        std::size_t count = 0;
        for (std::size_t i = 0; i < data_.size(); ++i) {
            // Count UTF-8 character start bytes
            if ((static_cast<unsigned char>(data_[i]) & 0xC0) != 0x80) {
                ++count;
            }
        }
        char_count_ = count;
    }
    return *char_count_;
}

auto UnicodeText::at(std::size_t char_index) const -> char32_t {
    if (char_index >= length()) {
        throw std::out_of_range("UnicodeText::at: character index out of range");
    }
    
    std::size_t byte_offset = 0;
    std::size_t current_char = 0;
    
    // Find the byte offset for the character
    while (byte_offset < data_.size() && current_char < char_index) {
        if ((static_cast<unsigned char>(data_[byte_offset]) & 0xC0) != 0x80) {
            ++current_char;
        }
        ++byte_offset;
    }
    
    if (byte_offset >= data_.size()) {
        return U'?';
    }
    
    // Decode UTF-8 character
    unsigned char byte = static_cast<unsigned char>(data_[byte_offset]);
    if (byte < 0x80) {
        return static_cast<char32_t>(byte);
    } else if ((byte & 0xE0) == 0xC0) {
        if (byte_offset + 1 >= data_.size()) return U'?';
        return static_cast<char32_t>(((byte & 0x1F) << 6) | 
                                   (static_cast<unsigned char>(data_[byte_offset + 1]) & 0x3F));
    } else if ((byte & 0xF0) == 0xE0) {
        if (byte_offset + 2 >= data_.size()) return U'?';
        return static_cast<char32_t>(((byte & 0x0F) << 12) | 
                                   ((static_cast<unsigned char>(data_[byte_offset + 1]) & 0x3F) << 6) |
                                   (static_cast<unsigned char>(data_[byte_offset + 2]) & 0x3F));
    } else if ((byte & 0xF8) == 0xF0) {
        if (byte_offset + 3 >= data_.size()) return U'?';
        return static_cast<char32_t>(((byte & 0x07) << 18) | 
                                   ((static_cast<unsigned char>(data_[byte_offset + 1]) & 0x3F) << 12) |
                                   ((static_cast<unsigned char>(data_[byte_offset + 2]) & 0x3F) << 6) |
                                   (static_cast<unsigned char>(data_[byte_offset + 3]) & 0x3F));
    }
    return U'?'; // Invalid UTF-8 sequence
}
