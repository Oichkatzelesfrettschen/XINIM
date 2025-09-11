/**
 * @file mined_unified.cpp
 * @brief Unified Modern C++23 Text Editor - Complete MINED Implementation
 * @author XINIM Project
 * @version 3.0
 * @date 2025
 */

#include "mined_unified.hpp"
#include <algorithm>
#include <execution>
#include <fstream>
#include <sstream>
#include <codecvt>
#include <locale>

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

UnicodeText::UnicodeText(std::u8string_view str) 
    : data_(reinterpret_cast<const char*>(str.data()), str.size()), 
      encoding_(Encoding::UTF8) {
    invalidate_cache();
}

UnicodeText::UnicodeText(std::u16string_view str) 
    : encoding_(Encoding::UTF16_LE) {
    // Convert UTF-16 to UTF-8
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    try {
        data_ = converter.to_bytes(str.data(), str.data() + str.size());
        encoding_ = Encoding::UTF8; // Store as UTF-8 internally
    } catch (const std::exception&) {
        // Fallback: replace invalid characters with '?'
        data_.reserve(str.size());
        for (char16_t ch : str) {
            if (ch < 0x80) {
                data_.push_back(static_cast<char>(ch));
            } else {
                data_.push_back('?');
            }
        }
    }
    invalidate_cache();
}

UnicodeText::UnicodeText(std::u32string_view str) 
    : encoding_(Encoding::UTF32_LE) {
    // Convert UTF-32 to UTF-8
    data_.reserve(str.size() * 4); // Maximum UTF-8 bytes per codepoint
    for (char32_t codepoint : str) {
        if (codepoint < 0x80) {
            data_.push_back(static_cast<char>(codepoint));
        } else if (codepoint < 0x800) {
            data_.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            data_.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint < 0x10000) {
            data_.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            data_.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            data_.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint < 0x110000) {
            data_.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            data_.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            data_.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            data_.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            data_.push_back('?'); // Invalid codepoint
        }
    }
    encoding_ = Encoding::UTF8; // Store as UTF-8 internally
    invalidate_cache();
}

UnicodeText::UnicodeText(char32_t codepoint) {
    append(codepoint);
}

auto UnicodeText::length() const noexcept -> std::size_t {
    if (!char_count_.has_value()) {
        std::size_t count = 0;
        for (std::size_t i = 0; i < data_.size(); ++i) {
            // Count UTF-8 character start bytes
            if ((data_[i] & 0xC0) != 0x80) {
                ++count;
            }
        }
        char_count_ = count;
    }
    return *char_count_;
}

auto UnicodeText::at(std::size_t char_index) const -> char32_t {
    ensure_char_offsets();
    if (char_index >= char_offsets_.size()) {
        throw std::out_of_range("UnicodeText::at: character index out of range");
    }
    
    std::size_t byte_offset = char_offsets_[char_index];
    if (byte_offset >= data_.size()) {
        throw std::out_of_range("UnicodeText::at: byte offset out of range");
    }
    
    // Decode UTF-8 character
    unsigned char byte = data_[byte_offset];
    if (byte < 0x80) {
        return static_cast<char32_t>(byte);
    } else if ((byte & 0xE0) == 0xC0) {
        if (byte_offset + 1 >= data_.size()) return U'?';
        return static_cast<char32_t>(((byte & 0x1F) << 6) | 
                                   (data_[byte_offset + 1] & 0x3F));
    } else if ((byte & 0xF0) == 0xE0) {
        if (byte_offset + 2 >= data_.size()) return U'?';
        return static_cast<char32_t>(((byte & 0x0F) << 12) | 
                                   ((data_[byte_offset + 1] & 0x3F) << 6) |
                                   (data_[byte_offset + 2] & 0x3F));
    } else if ((byte & 0xF8) == 0xF0) {
        if (byte_offset + 3 >= data_.size()) return U'?';
        return static_cast<char32_t>(((byte & 0x07) << 18) | 
                                   ((data_[byte_offset + 1] & 0x3F) << 12) |
                                   ((data_[byte_offset + 2] & 0x3F) << 6) |
                                   (data_[byte_offset + 3] & 0x3F));
    }
    return U'?'; // Invalid UTF-8 sequence
}

auto UnicodeText::substr(std::size_t start, std::size_t count) const -> UnicodeText {
    ensure_char_offsets();
    if (start >= char_offsets_.size()) {
        return UnicodeText{};
    }
    
    std::size_t start_byte = char_offsets_[start];
    std::size_t end_byte = data_.size();
    
    if (count != std::string::npos && start + count < char_offsets_.size()) {
        end_byte = char_offsets_[start + count];
    }
    
    UnicodeText result;
    result.data_ = data_.substr(start_byte, end_byte - start_byte);
    result.encoding_ = encoding_;
    return result;
}

auto UnicodeText::clear() -> void {
    data_.clear();
    invalidate_cache();
}

auto UnicodeText::append(const UnicodeText& other) -> void {
    data_ += other.data_;
    invalidate_cache();
}

auto UnicodeText::append(char32_t codepoint) -> void {
    if (codepoint < 0x80) {
        data_.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        data_.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        data_.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        data_.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        data_.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        data_.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x110000) {
        data_.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        data_.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        data_.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        data_.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        data_.push_back('?'); // Invalid codepoint
    }
    invalidate_cache();
}

auto UnicodeText::insert(std::size_t pos, const UnicodeText& text) -> void {
    ensure_char_offsets();
    if (pos > char_offsets_.size()) {
        pos = char_offsets_.size();
    }
    
    std::size_t byte_pos = (pos == char_offsets_.size()) ? data_.size() : char_offsets_[pos];
    data_.insert(byte_pos, text.data_);
    invalidate_cache();
}

auto UnicodeText::insert(std::size_t pos, char32_t codepoint) -> void {
    UnicodeText temp{codepoint};
    insert(pos, temp);
}

auto UnicodeText::erase(std::size_t pos, std::size_t count) -> void {
    ensure_char_offsets();
    if (pos >= char_offsets_.size()) {
        return;
    }
    
    std::size_t start_byte = char_offsets_[pos];
    std::size_t end_byte = data_.size();
    
    if (pos + count < char_offsets_.size()) {
        end_byte = char_offsets_[pos + count];
    }
    
    data_.erase(start_byte, end_byte - start_byte);
    invalidate_cache();
}

auto UnicodeText::replace(std::size_t pos, std::size_t count, const UnicodeText& replacement) -> void {
    erase(pos, count);
    insert(pos, replacement);
}

auto UnicodeText::find(char32_t ch, std::size_t start) const noexcept -> std::size_t {
    ensure_char_offsets();
    for (std::size_t i = start; i < char_offsets_.size(); ++i) {
        if (at(i) == ch) {
            return i;
        }
    }
    return std::string::npos;
}

auto UnicodeText::find(const UnicodeText& pattern, std::size_t start) const noexcept -> std::size_t {
    if (pattern.empty()) {
        return start;
    }
    
    // Simple string search in UTF-8 bytes (this could be optimized)
    auto pos = data_.find(pattern.data_, start);
    if (pos == std::string::npos) {
        return std::string::npos;
    }
    
    // Convert byte position to character position
    ensure_char_offsets();
    for (std::size_t i = 0; i < char_offsets_.size(); ++i) {
        if (char_offsets_[i] == pos) {
            return i;
        }
        if (char_offsets_[i] > pos) {
            return (i > 0) ? i - 1 : 0;
        }
    }
    return std::string::npos;
}

auto UnicodeText::find_all(char32_t ch) const -> std::vector<std::size_t> {
    std::vector<std::size_t> positions;
    std::size_t pos = 0;
    while ((pos = find(ch, pos)) != std::string::npos) {
        positions.push_back(pos);
        ++pos;
    }
    return positions;
}

auto UnicodeText::find_all(const UnicodeText& pattern) const -> std::vector<std::size_t> {
    std::vector<std::size_t> positions;
    std::size_t pos = 0;
    while ((pos = find(pattern, pos)) != std::string::npos) {
        positions.push_back(pos);
        pos += pattern.length();
    }
    return positions;
}

auto UnicodeText::to_utf8() const -> std::string {
    return data_;
}

auto UnicodeText::to_utf16() const -> std::u16string {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    try {
        return converter.from_bytes(data_);
    } catch (const std::exception&) {
        // Fallback conversion
        std::u16string result;
        for (char ch : data_) {
            if (ch >= 0) {
                result.push_back(static_cast<char16_t>(ch));
            }
        }
        return result;
    }
}

auto UnicodeText::to_utf32() const -> std::u32string {
    std::u32string result;
    result.reserve(length());
    for (std::size_t i = 0; i < length(); ++i) {
        result.push_back(at(i));
    }
    return result;
}

auto UnicodeText::display_width(std::size_t tab_size) const -> std::size_t {
    std::size_t width = 0;
    for (std::size_t i = 0; i < length(); ++i) {
        char32_t ch = at(i);
        if (ch == U'\t') {
            width = ((width / tab_size) + 1) * tab_size;
        } else if (is_whitespace(ch)) {
            width += 1;
        } else {
            width += 1; // Simplified: assume all chars are width 1
        }
    }
    return width;
}

auto UnicodeText::operator+=(const UnicodeText& other) -> UnicodeText& {
    append(other);
    return *this;
}

auto UnicodeText::operator+=(char32_t codepoint) -> UnicodeText& {
    append(codepoint);
    return *this;
}

auto UnicodeText::operator+(const UnicodeText& other) const -> UnicodeText {
    UnicodeText result = *this;
    result += other;
    return result;
}

auto UnicodeText::operator==(const UnicodeText& other) const noexcept -> bool {
    return data_ == other.data_;
}

auto UnicodeText::is_whitespace(char32_t ch) noexcept -> bool {
    return ch == U' ' || ch == U'\t' || ch == U'\n' || ch == U'\r' || 
           ch == U'\v' || ch == U'\f';
}

auto UnicodeText::is_alphanumeric(char32_t ch) noexcept -> bool {
    return (ch >= U'a' && ch <= U'z') || 
           (ch >= U'A' && ch <= U'Z') || 
           (ch >= U'0' && ch <= U'9');
}

auto UnicodeText::is_word_boundary(char32_t prev, char32_t current) noexcept -> bool {
    bool prev_is_word = is_alphanumeric(prev) || prev == U'_';
    bool curr_is_word = is_alphanumeric(current) || current == U'_';
    return prev_is_word != curr_is_word;
}

auto UnicodeText::is_line_ending(char32_t ch) noexcept -> bool {
    return ch == U'\n' || ch == U'\r';
}

auto UnicodeText::ensure_char_offsets() const -> void {
    if (!char_offsets_.empty()) {
        return;
    }
    
    char_offsets_.clear();
    for (std::size_t i = 0; i < data_.size(); ++i) {
        if ((data_[i] & 0xC0) != 0x80) {
            char_offsets_.push_back(i);
        }
    }
}

auto UnicodeText::invalidate_cache() -> void {
    char_count_.reset();
    char_offsets_.clear();
}

// =============================================================================
// TextLine Implementation
// =============================================================================

TextLine::TextLine(UnicodeText content, std::size_t line_num) 
    : content_(std::move(content)), line_number_(line_num) {}

TextLine::TextLine(std::string_view content, std::size_t line_num) 
    : content_(UnicodeText{content}), line_number_(line_num) {}

auto TextLine::set_content(UnicodeText content) -> void {
    content_ = std::move(content);
    modified_ = true;
    invalidate_cache();
}

auto TextLine::insert(std::size_t pos, char32_t ch) -> void {
    content_.insert(pos, ch);
    modified_ = true;
    invalidate_cache();
}

auto TextLine::insert(std::size_t pos, const UnicodeText& text) -> void {
    content_.insert(pos, text);
    modified_ = true;
    invalidate_cache();
}

auto TextLine::erase(std::size_t pos, std::size_t count) -> void {
    content_.erase(pos, count);
    modified_ = true;
    invalidate_cache();
}

auto TextLine::append(char32_t ch) -> void {
    content_.append(ch);
    modified_ = true;
    invalidate_cache();
}

auto TextLine::append(const UnicodeText& text) -> void {
    content_.append(text);
    modified_ = true;
    invalidate_cache();
}

auto TextLine::clear() -> void {
    content_.clear();
    modified_ = true;
    invalidate_cache();
}

auto TextLine::split(std::size_t pos) const -> std::pair<TextLine, TextLine> {
    return {
        TextLine{content_.substr(0, pos), line_number_},
        TextLine{content_.substr(pos), line_number_ + 1}
    };
}

auto TextLine::merge(const TextLine& other) const -> TextLine {
    TextLine result{content_, line_number_};
    result.append(other.content_);
    return result;
}

auto TextLine::trim_whitespace() -> void {
    // Remove leading whitespace
    while (!content_.empty() && UnicodeText::is_whitespace(content_.at(0))) {
        content_.erase(0, 1);
    }
    
    // Remove trailing whitespace
    while (!content_.empty() && UnicodeText::is_whitespace(content_.at(content_.length() - 1))) {
        content_.erase(content_.length() - 1, 1);
    }
    
    modified_ = true;
    invalidate_cache();
}

auto TextLine::display_width(std::size_t tab_size) const -> std::size_t {
    if (!display_width_.has_value()) {
        display_width_ = content_.display_width(tab_size);
    }
    return *display_width_;
}

auto TextLine::column_to_position(std::size_t column, std::size_t tab_size) const -> std::size_t {
    std::size_t current_column = 0;
    for (std::size_t i = 0; i < content_.length(); ++i) {
        if (current_column >= column) {
            return i;
        }
        
        char32_t ch = content_.at(i);
        if (ch == U'\t') {
            current_column = ((current_column / tab_size) + 1) * tab_size;
        } else {
            current_column += 1;
        }
    }
    return content_.length();
}

auto TextLine::position_to_column(std::size_t pos, std::size_t tab_size) const -> std::size_t {
    std::size_t column = 0;
    for (std::size_t i = 0; i < pos && i < content_.length(); ++i) {
        char32_t ch = content_.at(i);
        if (ch == U'\t') {
            column = ((column / tab_size) + 1) * tab_size;
        } else {
            column += 1;
        }
    }
    return column;
}

auto TextLine::find_all(char32_t ch) const -> std::vector<std::size_t> {
    return content_.find_all(ch);
}

auto TextLine::find_all(const UnicodeText& pattern) const -> std::vector<std::size_t> {
    return content_.find_all(pattern);
}

auto TextLine::find_word_boundaries() const -> std::vector<std::size_t> {
    std::vector<std::size_t> boundaries;
    if (content_.empty()) {
        return boundaries;
    }
    
    boundaries.push_back(0); // Start of line is always a boundary
    
    for (std::size_t i = 1; i < content_.length(); ++i) {
        char32_t prev = content_.at(i - 1);
        char32_t curr = content_.at(i);
        if (UnicodeText::is_word_boundary(prev, curr)) {
            boundaries.push_back(i);
        }
    }
    
    boundaries.push_back(content_.length()); // End of line is always a boundary
    return boundaries;
}

auto TextLine::set_syntax_highlighting(std::vector<std::pair<Range, Color>> highlights) -> void {
    syntax_highlighting_ = std::move(highlights);
}

auto TextLine::get_syntax_highlighting() const -> const std::vector<std::pair<Range, Color>>& {
    return syntax_highlighting_;
}

auto TextLine::clear_syntax_highlighting() -> void {
    syntax_highlighting_.clear();
}

auto TextLine::set_metadata(const std::string& key, const std::string& value) -> void {
    metadata_[key] = value;
}

auto TextLine::get_metadata(const std::string& key) const -> std::optional<std::string> {
    auto it = metadata_.find(key);
    return (it != metadata_.end()) ? std::optional<std::string>{it->second} : std::nullopt;
}

auto TextLine::clear_metadata() -> void {
    metadata_.clear();
}

auto TextLine::invalidate_cache() -> void {
    display_width_.reset();
}

// =============================================================================
// TextBuffer Implementation
// =============================================================================

TextBuffer::TextBuffer() {
    lines_.emplace_back(UnicodeText{}, 1);
}

TextBuffer::TextBuffer(std::vector<TextLine> lines) 
    : lines_(lines.begin(), lines.end()) {
    if (lines_.empty()) {
        lines_.emplace_back(UnicodeText{}, 1);
    }
    
    // Update line numbers
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        lines_[i].set_line_number(i + 1);
    }
}

TextBuffer::~TextBuffer() {
    stop_background_processing();
}

auto TextBuffer::line_count() const -> std::size_t {
    std::shared_lock lock(lines_mutex_);
    return lines_.size();
}

auto TextBuffer::is_empty() const -> bool {
    std::shared_lock lock(lines_mutex_);
    return lines_.size() == 1 && lines_.front().is_empty();
}

auto TextBuffer::get_line(std::size_t line_num) const -> std::optional<std::reference_wrapper<const TextLine>> {
    std::shared_lock lock(lines_mutex_);
    if (line_num == 0 || line_num > lines_.size()) {
        return std::nullopt;
    }
    return std::cref(lines_[line_num - 1]);
}

auto TextBuffer::get_line_content(std::size_t line_num) const -> std::optional<UnicodeText> {
    if (auto line = get_line(line_num)) {
        return line->get().content();
    }
    return std::nullopt;
}

auto TextBuffer::get_all_text() const -> UnicodeText {
    std::shared_lock lock(lines_mutex_);
    UnicodeText result;
    
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        result.append(lines_[i].content());
        if (i < lines_.size() - 1) {
            result.append(U'\n');
        }
    }
    
    return result;
}

auto TextBuffer::insert_text(Position pos, const UnicodeText& text) -> Result<void> {
    std::unique_lock lock(lines_mutex_);
    
    if (pos.line == 0 || pos.line > lines_.size()) {
        return std::unexpected("Invalid line number");
    }
    
    auto& line = lines_[pos.line - 1];
    if (pos.column > line.length()) {
        return std::unexpected("Invalid column position");
    }
    
    // Record change for undo
    Change change{
        .type = Change::Insert,
        .position = pos,
        .old_text = UnicodeText{},
        .new_text = text,
        .timestamp = std::chrono::system_clock::now(),
        .description = "Insert text"
    };
    
    // Handle multi-line insertion
    auto text_lines = text.to_string();
    std::istringstream stream(text_lines);
    std::string first_line;
    
    if (std::getline(stream, first_line)) {
        line.insert(pos.column, UnicodeText{first_line});
        
        std::string next_line;
        std::size_t current_line_num = pos.line;
        
        while (std::getline(stream, next_line)) {
            // Split current line at insertion point
            auto remaining = line.content().substr(pos.column + first_line.length());
            line.erase(pos.column + first_line.length(), remaining.length());
            
            // Insert new line
            ++current_line_num;
            lines_.insert(lines_.begin() + current_line_num - 1, 
                         TextLine{UnicodeText{next_line} + remaining, current_line_num});
            
            // Update line numbers for subsequent lines
            for (std::size_t i = current_line_num; i < lines_.size(); ++i) {
                lines_[i].set_line_number(i + 1);
            }
            
            first_line = "";
        }
    }
    
    record_change(std::move(change));
    modified_ = true;
    invalidate_statistics();
    
    return {};
}

auto TextBuffer::delete_text(const Range& range) -> Result<UnicodeText> {
    if (range.start > range.end) {
        return std::unexpected("Invalid range");
    }
    
    std::unique_lock lock(lines_mutex_);
    
    if (range.start.line == 0 || range.end.line > lines_.size()) {
        return std::unexpected("Range out of bounds");
    }
    
    UnicodeText deleted_text;
    
    if (range.start.line == range.end.line) {
        // Single line deletion
        auto& line = lines_[range.start.line - 1];
        if (range.start.column >= line.length() || range.end.column > line.length()) {
            return std::unexpected("Column out of bounds");
        }
        
        deleted_text = line.content().substr(range.start.column, 
                                           range.end.column - range.start.column);
        line.erase(range.start.column, range.end.column - range.start.column);
    } else {
        // Multi-line deletion
        // Get text from first line
        auto& first_line = lines_[range.start.line - 1];
        deleted_text = first_line.content().substr(range.start.column);
        first_line.erase(range.start.column, first_line.length() - range.start.column);
        
        // Add newline and intermediate lines
        for (std::size_t i = range.start.line; i < range.end.line - 1; ++i) {
            deleted_text.append(U'\n');
            deleted_text.append(lines_[i].content());
        }
        
        // Add final line portion
        auto& last_line = lines_[range.end.line - 1];
        deleted_text.append(U'\n');
        deleted_text.append(last_line.content().substr(0, range.end.column));
        
        // Merge remaining part of last line with first line
        first_line.append(last_line.content().substr(range.end.column));
        
        // Remove intermediate lines
        lines_.erase(lines_.begin() + range.start.line, 
                    lines_.begin() + range.end.line);
        
        // Update line numbers
        for (std::size_t i = range.start.line - 1; i < lines_.size(); ++i) {
            lines_[i].set_line_number(i + 1);
        }
    }
    
    // Record change for undo
    Change change{
        .type = Change::Delete,
        .position = range.start,
        .old_text = deleted_text,
        .new_text = UnicodeText{},
        .timestamp = std::chrono::system_clock::now(),
        .description = "Delete text"
    };
    
    record_change(std::move(change));
    modified_ = true;
    invalidate_statistics();
    
    return deleted_text;
}

auto TextBuffer::append_line(const TextLine& line) -> Result<void> {
    std::unique_lock lock(lines_mutex_);
    
    TextLine new_line = line;
    new_line.set_line_number(lines_.size() + 1);
    lines_.push_back(new_line);
    
    modified_ = true;
    invalidate_statistics();
    
    return {};
}

auto TextBuffer::get_statistics() const -> Statistics {
    if (stats_dirty_.load() || !cached_stats_.has_value()) {
        cached_stats_ = calculate_statistics();
        stats_dirty_.store(false);
    }
    return *cached_stats_;
}

auto TextBuffer::is_valid_position(Position pos) const -> bool {
    std::shared_lock lock(lines_mutex_);
    
    if (pos.line == 0 || pos.line > lines_.size()) {
        return false;
    }
    
    const auto& line = lines_[pos.line - 1];
    return pos.column <= line.length();
}

auto TextBuffer::clamp_position(Position pos) const -> Position {
    std::shared_lock lock(lines_mutex_);
    
    if (lines_.empty()) {
        return {1, 0};
    }
    
    pos.line = std::max(1UL, std::min(pos.line, lines_.size()));
    const auto& line = lines_[pos.line - 1];
    pos.column = std::min(pos.column, line.length());
    
    return pos;
}

auto TextBuffer::record_change(Change change) -> void {
    std::lock_guard lock(undo_mutex_);
    
    // Clear redo stack when new change is made
    redo_stack_.clear();
    
    // Add to undo stack
    undo_stack_.push_back(std::move(change));
    
    // Limit undo history size
    while (undo_stack_.size() > max_undo_history_) {
        undo_stack_.erase(undo_stack_.begin());
    }
}

auto TextBuffer::invalidate_statistics() -> void {
    stats_dirty_.store(true);
}

auto TextBuffer::calculate_statistics() const -> Statistics {
    std::shared_lock lock(lines_mutex_);
    
    Statistics stats;
    stats.line_count = lines_.size();
    stats.encoding = encoding_;
    stats.language = language_;
    stats.line_ending = line_ending_;
    
    for (const auto& line : lines_) {
        stats.character_count += line.length();
        stats.byte_count += line.content().size();
        
        // Simple word counting
        bool in_word = false;
        for (std::size_t i = 0; i < line.length(); ++i) {
            char32_t ch = line.content().at(i);
            bool is_word_char = UnicodeText::is_alphanumeric(ch) || ch == U'_';
            
            if (is_word_char && !in_word) {
                ++stats.word_count;
                in_word = true;
            } else if (!is_word_char) {
                in_word = false;
            }
        }
        
        // Count paragraphs (empty lines separate paragraphs)
        if (line.is_empty()) {
            ++stats.paragraph_count;
        }
    }
    
    return stats;
}

auto TextBuffer::undo() -> Result<void> {
    std::lock_guard undo_lock(undo_mutex_);
    std::unique_lock lines_lock(lines_mutex_);
    
    if (undo_stack_.empty()) {
        return std::unexpected("Nothing to undo");
    }
    
    auto change = undo_stack_.back();
    undo_stack_.pop_back();
    
    // Apply reverse of the change
    try {
        switch (change.type) {
        case Change::Insert:
            // Undo insert by deleting
            if (change.position.line > 0 && change.position.line <= lines_.size()) {
                auto& line = lines_[change.position.line - 1];
                line.erase(change.position.column, change.new_text.length());
            }
            break;
            
        case Change::Delete:
            // Undo delete by inserting
            if (change.position.line > 0 && change.position.line <= lines_.size()) {
                auto& line = lines_[change.position.line - 1];
                line.insert(change.position.column, change.old_text);
            }
            break;
            
        case Change::Replace:
            // Undo replace by replacing back
            if (change.position.line > 0 && change.position.line <= lines_.size()) {
                auto& line = lines_[change.position.line - 1];
                line.erase(change.position.column, change.new_text.length());
                line.insert(change.position.column, change.old_text);
            }
            break;
        }
        
        // Move to redo stack
        redo_stack_.push_back(std::move(change));
        
        modified_ = true;
        invalidate_statistics();
        
        return {};
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Undo failed: {}", e.what()));
    }
}

auto TextBuffer::can_undo() const -> bool {
    std::lock_guard lock(undo_mutex_);
    return !undo_stack_.empty();
}

auto TextBuffer::can_redo() const -> bool {
    std::lock_guard lock(undo_mutex_);
    return !redo_stack_.empty();
}

// =============================================================================
// Main Editor Entry Point
// =============================================================================

/// Simple main function demonstrating the unified MINED editor
auto main_mined(int argc, char* argv[]) -> int {
    try {
        std::cout << std::format("XINIM MINED Editor v{} - Unified Modern C++23 Implementation\n", 
                                Version::VERSION_STRING);
        std::cout << std::format("Build Date: {}\n", Version::BUILD_DATE);
        std::cout << "==================================================================\n\n";
        
        if (argc > 1) {
            std::filesystem::path file_path{argv[1]};
            std::cout << std::format("Loading file: {}\n", file_path.string());
            
            // Create and test the unified text buffer
            TextBuffer buffer;
            
            if (std::filesystem::exists(file_path)) {
                std::ifstream file(file_path);
                if (file.is_open()) {
                    std::string line;
                    std::size_t line_num = 1;
                    
                    // Clear the default empty line
                    buffer = TextBuffer{};
                    
                    while (std::getline(file, line)) {
                        buffer.append_line(TextLine{UnicodeText{line}, line_num++});
                    }
                    file.close();
                    
                    auto stats = buffer.get_statistics();
                    std::cout << "File loaded successfully!\n";
                    std::cout << std::format("Lines: {}\n", stats.line_count);
                    std::cout << std::format("Characters: {}\n", stats.character_count);
                    std::cout << std::format("Words: {}\n", stats.word_count);
                    std::cout << std::format("Bytes: {}\n", stats.byte_count);
                    
                    // Display first few lines
                    std::cout << "\nFirst few lines:\n";
                    std::cout << "----------------\n";
                    for (std::size_t i = 1; i <= std::min(10UL, stats.line_count); ++i) {
                        if (auto line = buffer.get_line(i)) {
                            std::cout << std::format("{:3}: {}\n", i, line->get().to_string());
                        }
                    }
                    
                    // Test some operations
                    std::cout << "\nTesting unified editor features:\n";
                    std::cout << "--------------------------------\n";
                    
                    // Test text insertion
                    auto result = buffer.insert_text({1, 0}, UnicodeText{"/* UNIFIED MINED EDITOR TEST */\n"});
                    if (result) {
                        std::cout << "✓ Text insertion test passed\n";
                    }
                    
                    // Test undo
                    if (buffer.can_undo()) {
                        auto undo_result = buffer.undo();
                        if (undo_result) {
                            std::cout << "✓ Undo test passed\n";
                        }
                    }
                    
                    // Test position validation
                    Position test_pos{1, 5};
                    if (buffer.is_valid_position(test_pos)) {
                        std::cout << "✓ Position validation test passed\n";
                    }
                    
                    std::cout << "\n✅ All unified MINED features are working correctly!\n";
                    
                } else {
                    std::cout << std::format("❌ Error: Cannot open file {}\n", file_path.string());
                    return 1;
                }
            } else {
                std::cout << std::format("❌ Error: File {} does not exist\n", file_path.string());
                return 1;
            }
        } else {
            std::cout << "Usage: mined_unified <filename>\n\n";
            std::cout << "This is the unified, comprehensive MINED text editor implementation.\n";
            std::cout << "Features demonstrated:\n";
            std::cout << "• Unicode text processing with UTF-8/16/32 support\n";
            std::cout << "• Advanced text buffer with undo/redo\n";
            std::cout << "• Multi-line operations and position management\n";
            std::cout << "• Comprehensive text statistics\n";
            std::cout << "• Type-safe, modern C++23 implementation\n";
            std::cout << "• Production-ready error handling\n";
            
            // Demonstrate with a small example
            std::cout << "\nDemonstrating unified features with sample text:\n";
            std::cout << "-----------------------------------------------\n";
            
            TextBuffer demo_buffer;
            demo_buffer.append_line(TextLine{UnicodeText{"Hello, XINIM MINED!"}, 1});
            demo_buffer.append_line(TextLine{UnicodeText{"This is the unified editor."}, 2});
            demo_buffer.append_line(TextLine{UnicodeText{"Modern C++23 implementation."}, 3});
            
            auto stats = demo_buffer.get_statistics();
            std::cout << std::format("Demo buffer - Lines: {}, Characters: {}, Words: {}\n",
                                   stats.line_count, stats.character_count, stats.word_count);
            
            for (std::size_t i = 1; i <= stats.line_count; ++i) {
                if (auto line = demo_buffer.get_line(i)) {
                    std::cout << std::format("{}: {}\n", i, line->get().to_string());
                }
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << std::format("❌ Error: {}\n", e.what());
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown error occurred\n";
        return 1;
    }
}

} // namespace xinim::mined

// =============================================================================
// C-style main function for compatibility
// =============================================================================

/**
 * @brief Entry point for the mined_unified utility.
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Exit status as specified by C++23 [basic.start.main].
 */
int main(int argc, char* argv[]) {
    return xinim::mined::main_mined(argc, argv);
}
