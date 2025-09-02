/**
 * @file mined_final.cpp
 * @brief Final Unified MINED Editor - Production Ready C++20 Implementation
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

auto UnicodeText::substr(std::size_t start, std::size_t count) const -> UnicodeText {
    if (start >= length()) {
        return UnicodeText{};
    }
    
    // Find byte positions for start and end
    std::size_t start_byte = 0;
    std::size_t current_char = 0;
    
    while (start_byte < data_.size() && current_char < start) {
        if ((static_cast<unsigned char>(data_[start_byte]) & 0xC0) != 0x80) {
            ++current_char;
        }
        ++start_byte;
    }
    
    if (count == std::string::npos) {
        UnicodeText result;
        result.data_ = data_.substr(start_byte);
        result.encoding_ = encoding_;
        return result;
    }
    
    std::size_t end_byte = start_byte;
    std::size_t chars_counted = 0;
    
    while (end_byte < data_.size() && chars_counted < count) {
        if ((static_cast<unsigned char>(data_[end_byte]) & 0xC0) != 0x80) {
            ++chars_counted;
        }
        ++end_byte;
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
    if (pos >= length()) {
        append(text);
        return;
    }
    
    // Find byte position
    std::size_t byte_pos = 0;
    std::size_t current_char = 0;
    
    while (byte_pos < data_.size() && current_char < pos) {
        if ((static_cast<unsigned char>(data_[byte_pos]) & 0xC0) != 0x80) {
            ++current_char;
        }
        ++byte_pos;
    }
    
    data_.insert(byte_pos, text.data_);
    invalidate_cache();
}

auto UnicodeText::insert(std::size_t pos, char32_t codepoint) -> void {
    UnicodeText temp{codepoint};
    insert(pos, temp);
}

auto UnicodeText::erase(std::size_t pos, std::size_t count) -> void {
    if (pos >= length()) {
        return;
    }
    
    // Find start byte position
    std::size_t start_byte = 0;
    std::size_t current_char = 0;
    
    while (start_byte < data_.size() && current_char < pos) {
        if ((static_cast<unsigned char>(data_[start_byte]) & 0xC0) != 0x80) {
            ++current_char;
        }
        ++start_byte;
    }
    
    // Find end byte position
    std::size_t end_byte = start_byte;
    std::size_t chars_to_erase = 0;
    
    while (end_byte < data_.size() && chars_to_erase < count) {
        if ((static_cast<unsigned char>(data_[end_byte]) & 0xC0) != 0x80) {
            ++chars_to_erase;
        }
        ++end_byte;
    }
    
    data_.erase(start_byte, end_byte - start_byte);
    invalidate_cache();
}

auto UnicodeText::find(char32_t ch, std::size_t start) const noexcept -> std::size_t {
    for (std::size_t i = start; i < length(); ++i) {
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
    
    // Simple string search in UTF-8 bytes
    auto pos = data_.find(pattern.data_, start);
    if (pos == std::string::npos) {
        return std::string::npos;
    }
    
    // Convert byte position to character position
    std::size_t char_pos = 0;
    for (std::size_t i = 0; i < pos; ++i) {
        if ((static_cast<unsigned char>(data_[i]) & 0xC0) != 0x80) {
            ++char_pos;
        }
    }
    return char_pos;
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

auto UnicodeText::invalidate_cache() -> void {
    char_count_.reset();
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
        return Result<void>("Invalid line number");
    }
    
    auto& line = lines_[pos.line - 1];
    if (pos.column > line.length()) {
        return Result<void>("Invalid column position");
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
    auto text_str = text.to_string();
    std::istringstream stream(text_str);
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
    
    return Result<void>();
}

auto TextBuffer::insert_char(Position pos, char32_t ch) -> Result<void> {
    return insert_text(pos, UnicodeText{ch});
}

auto TextBuffer::delete_text(const Range& range) -> Result<UnicodeText> {
    if (range.start > range.end) {
        return Result<UnicodeText>("Invalid range");
    }
    
    std::unique_lock lock(lines_mutex_);
    
    if (range.start.line == 0 || range.end.line > lines_.size()) {
        return Result<UnicodeText>("Range out of bounds");
    }
    
    UnicodeText deleted_text;
    
    if (range.start.line == range.end.line) {
        // Single line deletion
        auto& line = lines_[range.start.line - 1];
        if (range.start.column >= line.length() || range.end.column > line.length()) {
            return Result<UnicodeText>("Column out of bounds");
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
    
    return Result<UnicodeText>(deleted_text);
}

auto TextBuffer::append_line(const TextLine& line) -> Result<void> {
    std::unique_lock lock(lines_mutex_);
    
    TextLine new_line = line;
    new_line.set_line_number(lines_.size() + 1);
    lines_.push_back(new_line);
    
    modified_ = true;
    invalidate_statistics();
    
    return Result<void>();
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

auto TextBuffer::can_undo() const -> bool {
    std::lock_guard lock(undo_mutex_);
    return !undo_stack_.empty();
}

auto TextBuffer::can_redo() const -> bool {
    std::lock_guard lock(undo_mutex_);
    return !redo_stack_.empty();
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
    }
    
    return stats;
}

auto TextBuffer::detect_language(const std::filesystem::path& path) const -> Language {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".c") return Language::C;
    if (extension == ".cpp" || extension == ".cxx" || extension == ".cc") return Language::CPP;
    if (extension == ".py") return Language::Python;
    if (extension == ".js") return Language::JavaScript;
    if (extension == ".rs") return Language::Rust;
    if (extension == ".s" || extension == ".asm") return Language::Assembly;
    
    return Language::PlainText;
}

auto TextBuffer::set_language(Language lang) -> void {
    language_ = lang;
}

auto TextBuffer::set_encoding(Encoding enc) -> void {
    encoding_ = enc;
}

// =============================================================================
// Cursor Implementation
// =============================================================================

auto Cursor::move_to(Position pos) -> bool {
    if (!buffer_->is_valid_position(pos)) {
        pos = buffer_->clamp_position(pos);
    }
    
    position_ = pos;
    desired_column_ = pos;
    return true;
}

auto Cursor::move_up(std::size_t count) -> bool {
    if (position_.line <= count) {
        position_.line = 1;
    } else {
        position_.line -= count;
    }
    
    // Try to maintain desired column
    if (auto line = buffer_->get_line(position_.line)) {
        position_.column = std::min(desired_column_.column, line->get().length());
    }
    
    return true;
}

auto Cursor::move_down(std::size_t count) -> bool {
    position_.line = std::min(position_.line + count, buffer_->line_count());
    
    // Try to maintain desired column
    if (auto line = buffer_->get_line(position_.line)) {
        position_.column = std::min(desired_column_.column, line->get().length());
    }
    
    return true;
}

auto Cursor::move_left(std::size_t count) -> bool {
    if (position_.column >= count) {
        position_.column -= count;
    } else if (position_.line > 1) {
        // Move to end of previous line
        --position_.line;
        if (auto line = buffer_->get_line(position_.line)) {
            position_.column = line->get().length();
        }
    } else {
        position_.column = 0;
    }
    
    desired_column_ = position_;
    return true;
}

auto Cursor::move_right(std::size_t count) -> bool {
    if (auto line = buffer_->get_line(position_.line)) {
        if (position_.column + count <= line->get().length()) {
            position_.column += count;
        } else if (position_.line < buffer_->line_count()) {
            // Move to start of next line
            ++position_.line;
            position_.column = 0;
        } else {
            position_.column = line->get().length();
        }
    }
    
    desired_column_ = position_;
    return true;
}

auto Cursor::move_to_line_start() -> bool {
    position_.column = 0;
    desired_column_ = position_;
    return true;
}

auto Cursor::move_to_line_end() -> bool {
    if (auto line = buffer_->get_line(position_.line)) {
        position_.column = line->get().length();
    }
    desired_column_ = position_;
    return true;
}

// =============================================================================
// MinedEditor Implementation
// =============================================================================

MinedEditor::MinedEditor(EditorConfig config) 
    : config_(std::move(config)) {
    auto result = initialize();
    if (!result) {
        throw std::runtime_error(std::format("Failed to initialize editor: {}", result.error()));
    }
}

auto MinedEditor::initialize() -> Result<void> {
    try {
        buffer_ = std::make_unique<TextBuffer>();
        cursor_ = std::make_unique<Cursor>(*buffer_);
        
        state_.current_file = "";
        state_.is_modified = false;
        state_.is_read_only = false;
        state_.status_message = "MINED Editor Ready";
        
        return Result<void>();
    } catch (const std::exception& e) {
        return Result<void>(std::format("Initialization failed: {}", e.what()));
    }
}

auto MinedEditor::load_file(const std::filesystem::path& path) -> Result<void> {
    try {
        if (!std::filesystem::exists(path)) {
            return Result<void>("File does not exist");
        }
        
        std::ifstream file(path);
        if (!file.is_open()) {
            return Result<void>("Cannot open file");
        }
        
        std::vector<TextLine> lines;
        std::string line_str;
        std::size_t line_num = 1;
        
        while (std::getline(file, line_str)) {
            lines.emplace_back(UnicodeText{line_str}, line_num++);
        }
        
        if (lines.empty()) {
            lines.emplace_back(UnicodeText{}, 1);
        }
        
        // Create new buffer with loaded content
        buffer_ = std::make_unique<TextBuffer>(std::move(lines));
        cursor_ = std::make_unique<Cursor>(*buffer_);
        
        // Set language based on file extension
        buffer_->set_language(buffer_->detect_language(path));
        
        state_.current_file = path;
        state_.is_modified = false;
        state_.status_message = std::format("Loaded {}", path.filename().string());
        
        return Result<void>();
    } catch (const std::exception& e) {
        return Result<void>(std::format("Failed to load file: {}", e.what()));
    }
}

auto MinedEditor::save_file(std::optional<std::filesystem::path> path) -> Result<void> {
    try {
        std::filesystem::path save_path = path ? *path : state_.current_file;
        
        if (save_path.empty()) {
            return Result<void>("No file path specified");
        }
        
        std::ofstream file(save_path);
        if (!file.is_open()) {
            return Result<void>("Cannot open file for writing");
        }
        
        auto stats = buffer_->get_statistics();
        for (std::size_t i = 1; i <= stats.line_count; ++i) {
            if (auto line = buffer_->get_line(i)) {
                file << line->get().to_string();
                if (i < stats.line_count) {
                    file << "\n";
                }
            }
        }
        
        state_.current_file = save_path;
        state_.is_modified = false;
        state_.last_save_time = std::chrono::system_clock::now();
        state_.status_message = std::format("Saved {}", save_path.filename().string());
        
        return Result<void>();
    } catch (const std::exception& e) {
        return Result<void>(std::format("Failed to save file: {}", e.what()));
    }
}

auto MinedEditor::insert_char(char32_t ch) -> Result<void> {
    auto pos = cursor_->position();
    auto result = buffer_->insert_char(pos, ch);
    if (result) {
        cursor_->move_right();
        state_.is_modified = true;
        update_state();
    }
    return result;
}

auto MinedEditor::new_line() -> Result<void> {
    auto pos = cursor_->position();
    auto result = buffer_->insert_char(pos, U'\n');
    if (result) {
        cursor_->move_down();
        cursor_->move_to_line_start();
        state_.is_modified = true;
        update_state();
    }
    return result;
}

auto MinedEditor::move_cursor_up() -> Result<void> {
    cursor_->move_up();
    update_state();
    return Result<void>();
}

auto MinedEditor::move_cursor_down() -> Result<void> {
    cursor_->move_down();
    update_state();
    return Result<void>();
}

auto MinedEditor::move_cursor_left() -> Result<void> {
    cursor_->move_left();
    update_state();
    return Result<void>();
}

auto MinedEditor::move_cursor_right() -> Result<void> {
    cursor_->move_right();
    update_state();
    return Result<void>();
}

auto MinedEditor::get_state() const -> EditorState {
    std::lock_guard lock(state_mutex_);
    return state_;
}

auto MinedEditor::get_config() const -> const EditorConfig& {
    return config_;
}

auto MinedEditor::get_buffer_statistics() const -> TextBuffer::Statistics {
    return buffer_->get_statistics();
}

auto MinedEditor::has_unsaved_changes() const -> bool {
    return state_.is_modified;
}

auto MinedEditor::cursor_position() const -> Position {
    return cursor_->position();
}

auto MinedEditor::update_state() -> void {
    std::lock_guard lock(state_mutex_);
    // Update state based on current editor state
    auto pos = cursor_->position();
    state_.status_message = std::format("Line {}, Column {}", pos.line, pos.column);
}

auto MinedEditor::display_status() const -> void {
    auto pos = cursor_position();
    auto stats = get_buffer_statistics();
    
    std::cout << std::format("\n[MINED v{} - Line {}/{}, Col {}, {} chars, {} words]",
                           Version::VERSION_STRING, pos.line, stats.line_count, 
                           pos.column, stats.character_count, stats.word_count);
    
    if (has_unsaved_changes()) {
        std::cout << " [Modified]";
    }
    
    if (!state_.current_file.empty()) {
        std::cout << std::format(" - {}", state_.current_file.filename().string());
    }
    
    std::cout << "\n";
}

auto MinedEditor::run() -> Result<void> {
    running_ = true;
    
    std::cout << std::format("XINIM MINED Editor v{} - Final Unified Implementation\n", 
                           Version::VERSION_STRING);
    std::cout << "===========================================================\n\n";
    
    display_status();
    
    // Simple command loop for demonstration
    return handle_simple_commands();
}

auto MinedEditor::handle_simple_commands() -> Result<void> {
    std::cout << "\nSimple Commands (type 'help' for full list):\n";
    std::cout << "  q, quit, :q - Quit\n";
    std::cout << "  :w - Save\n";
    std::cout << "  :i <text> - Insert text at cursor\n";
    std::cout << "  :s - Show statistics\n";
    std::cout << "  help, :help - Show help\n\n";
    
    std::string command;
    while (running_ && !should_quit_) {
        std::cout << "mined> ";
        std::getline(std::cin, command);
        
        if (command.empty()) continue;
        
        if (command == ":q" || command == ":quit" || command == "quit" || command == "q") {
            if (has_unsaved_changes()) {
                std::cout << "Warning: Unsaved changes! Use :q! to force quit.\n";
                continue;
            }
            should_quit_ = true;
            break;
        } else if (command == ":q!" || command == "quit!") {
            should_quit_ = true;
            break;
        } else if (command == ":w" || command == ":save") {
            auto result = save_file();
            if (result) {
                std::cout << "File saved successfully.\n";
            } else {
                std::cout << std::format("Save failed: {}\n", result.error());
            }
        } else if (command.starts_with(":i ")) {
            auto text = command.substr(3);
            for (char ch : text) {
                insert_char(static_cast<char32_t>(ch));
            }
            std::cout << "Text inserted.\n";
            display_status();
        } else if (command == ":s" || command == ":stats") {
            auto stats = get_buffer_statistics();
            std::cout << std::format("Statistics:\n");
            std::cout << std::format("  Lines: {}\n", stats.line_count);
            std::cout << std::format("  Characters: {}\n", stats.character_count);
            std::cout << std::format("  Words: {}\n", stats.word_count);
            std::cout << std::format("  Bytes: {}\n", stats.byte_count);
            std::cout << std::format("  Language: {}\n", static_cast<int>(stats.language));
        } else if (command == ":help" || command == "help") {
            std::cout << "\nMINED Editor Commands:\n";
            std::cout << "=====================\n";
            std::cout << "  q, quit, :q, :quit - Quit editor\n";
            std::cout << "  quit!, :q!         - Force quit (ignore unsaved changes)\n";
            std::cout << "  :w, :save          - Save current file\n";
            std::cout << "  :i <text>          - Insert text at cursor position\n";
            std::cout << "  :s, :stats         - Show buffer statistics\n";
            std::cout << "  help, :help        - Show this help\n";
            std::cout << "\nThis is the unified MINED editor demonstrating core features.\n";
        } else {
            std::cout << std::format("Unknown command: {}\n", command);
            std::cout << "Type ':help' for available commands.\n";
        }
    }
    
    running_ = false;
    return Result<void>();
}

// =============================================================================
// Factory and Main Functions
// =============================================================================

auto create_editor(EditorConfig config) -> std::unique_ptr<MinedEditor> {
    return std::make_unique<MinedEditor>(std::move(config));
}

auto main_mined(int argc, char* argv[]) -> int {
    try {
        std::cout << std::format("XINIM MINED Editor v{} - Final Unified Implementation\n", 
                                Version::VERSION_STRING);
        std::cout << "=============================================================\n\n";
        
        auto editor = create_editor();
        
        if (argc > 1) {
            std::filesystem::path file_path{argv[1]};
            std::cout << std::format("Loading file: {}\n", file_path.string());
            
            auto result = editor->load_file(file_path);
            if (!result) {
                std::cout << std::format("Failed to load file: {}\n", result.error());
                std::cout << "Starting with empty buffer.\n";
            }
        }
        
        auto run_result = editor->run();
        if (!run_result) {
            std::cerr << std::format("Editor error: {}\n", run_result.error());
            return 1;
        }
        
        std::cout << "\nMINED Editor session ended. Thank you!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << std::format("Fatal error: {}\n", e.what());
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred\n";
        return 1;
    }
}

} // namespace xinim::mined

// =============================================================================
// C-style main function for compatibility
// =============================================================================

int main(int argc, char* argv[]) {
    return xinim::mined::main_mined(argc, argv);
}
