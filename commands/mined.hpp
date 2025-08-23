/**
 * @file modern_mined.hpp
 * @brief Modern C++23 Text Editor - Reimplemented MINED for XINIM
 * @author XINIM Project
 * @version 2.0
 * @date 2025
 *
 * Complete rewrite of the MINED text editor using modern C++23 features:
 * - RAII memory management with smart pointers
 * - Type-safe enums and concepts
 * - Template-based generic programming
 * - Exc    [[nodiscard]] std::vector<Position> find_all(const text::UnicodeString &pattern)
 * const;ption-safe operations
 * - Unicode support with UTF-8/UTF-16/UTF-32
 * - SIMD-optimized text processing
 * - Async I/O operations
 * - Modern STL containers and algorithms
 * - Coroutines for responsive UI
 * - Module-based architecture
 */

#pragma once

// SIMD math operations will be implemented inline
#include <array>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <deque>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <future>
// #include <generator>  // Not yet available in all compilers
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>
#include <regex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace xinim::editor {

// =============================================================================
// Task/Coroutine Support (Simplified until full C++23 support)
// =============================================================================

template <typename T> class Task {
  public:
    using value_type = T;

    Task() = default;
    explicit Task(std::future<T> future) : future_(std::move(future)) {}

    auto get() -> T { return future_.get(); }
    auto valid() const -> bool { return future_.valid(); }
    auto wait() -> void { future_.wait(); }

    template <typename Rep, typename Period>
    auto wait_for(const std::chrono::duration<Rep, Period> &timeout) {
        return future_.wait_for(timeout);
    }

  private:
    std::future<T> future_;
};

// Specialization for void
template <> class Task<void> {
  public:
    using value_type = void;

    Task() = default;
    explicit Task(std::future<void> future) : future_(std::move(future)) {}

    auto get() -> void { future_.get(); }
    auto valid() const -> bool { return future_.valid(); }
    auto wait() -> void { future_.wait(); }

    template <typename Rep, typename Period>
    auto wait_for(const std::chrono::duration<Rep, Period> &timeout) {
        return future_.wait_for(timeout);
    }

  private:
    std::future<void> future_;
};

// Forward declarations
class TextBuffer;
class TextLine;
class Cursor;
class Display;
class CommandProcessor;
class EditorEngine;

/**
 * @brief Modern C++23 concepts for type safety
 */
namespace concepts {
template <typename T>
concept Printable = requires(const T &t) {
    { std::format("{}", t) } -> std::convertible_to<std::string>;
};

template <typename T>
concept TextContainer = requires(T container) {
    typename T::value_type;
    requires std::same_as<typename T::value_type, char> ||
                 std::same_as<typename T::value_type, char8_t> ||
                 std::same_as<typename T::value_type, char16_t> ||
                 std::same_as<typename T::value_type, char32_t>;
    { container.begin() } -> std::forward_iterator;
    { container.end() } -> std::forward_iterator;
    { container.size() } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept CommandHandler = requires(T handler) {
    { handler() } -> std::same_as<void>;
};
} // namespace concepts

/**
 * @brief Result types with comprehensive error handling
 */
enum class ResultCode : std::int32_t {
    Success = 0,
    InvalidInput = -1,
    EndOfFile = -2,
    OutOfMemory = -3,
    FileNotFound = -4,
    PermissionDenied = -5,
    InvalidOperation = -6,
    BufferFull = -7,
    UnknownError = -1000
};

template <typename T> using Result = std::expected<T, ResultCode>;

/**
 * @brief Modern position and coordinate types
 */
struct Position {
    std::size_t line{0};
    std::size_t column{0};

    constexpr auto operator<=>(const Position &) const = default;
    constexpr bool operator==(const Position &) const = default;
};

struct ScreenCoordinate {
    std::int32_t x{0};
    std::int32_t y{0};

    constexpr auto operator<=>(const ScreenCoordinate &) const = default;
    constexpr bool operator==(const ScreenCoordinate &) const = default;
};

struct DisplayRegion {
    ScreenCoordinate top_left{0, 0};
    ScreenCoordinate bottom_right{79, 23};

    [[nodiscard]] constexpr std::int32_t width() const noexcept {
        return bottom_right.x - top_left.x + 1;
    }

    [[nodiscard]] constexpr std::int32_t height() const noexcept {
        return bottom_right.y - top_left.y + 1;
    }

    [[nodiscard]] constexpr bool contains(ScreenCoordinate coord) const noexcept {
        return coord.x >= top_left.x && coord.x <= bottom_right.x && coord.y >= top_left.y &&
               coord.y <= bottom_right.y;
    }
};

/**
 * @brief Unicode-aware text processing
 */
namespace text {
enum class Encoding : std::uint8_t { UTF8 = 0, UTF16 = 1, UTF32 = 2, ASCII = 3 };

class UnicodeString {
  private:
    std::string data_;
    Encoding encoding_{Encoding::UTF8};
    mutable std::optional<std::size_t> char_count_;

  public:
    UnicodeString() = default;
    explicit UnicodeString(std::string_view str, Encoding enc = Encoding::UTF8);
    explicit UnicodeString(const char *str);
    explicit UnicodeString(std::u8string_view str);
    explicit UnicodeString(std::u16string_view str);
    explicit UnicodeString(std::u32string_view str);

    // Iterator support
    class const_iterator;
    class iterator;

    [[nodiscard]] const_iterator begin() const noexcept;
    [[nodiscard]] const_iterator end() const noexcept;
    [[nodiscard]] iterator begin() noexcept;
    [[nodiscard]] iterator end() noexcept;

    // Capacity
    [[nodiscard]] std::size_t size() const noexcept;   // Byte size
    [[nodiscard]] std::size_t length() const noexcept; // Character count
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;

    // Element access
    [[nodiscard]] char32_t at(std::size_t pos) const;
    [[nodiscard]] char32_t operator[](std::size_t pos) const;
    [[nodiscard]] std::string_view data() const noexcept { return data_; }
    [[nodiscard]] const char *c_str() const noexcept { return data_.c_str(); }

    // Modifiers
    void append(const UnicodeString &str);
    void append(char32_t ch);
    void insert(std::size_t pos, const UnicodeString &str);
    void insert(std::size_t pos, char32_t ch);
    void erase(std::size_t pos, std::size_t count = 1);
    void clear() noexcept;

    // String operations
    [[nodiscard]] UnicodeString substr(std::size_t pos,
                                       std::size_t count = std::string::npos) const;
    [[nodiscard]] std::size_t find(const UnicodeString &str, std::size_t pos = 0) const noexcept;
    [[nodiscard]] std::size_t find(char32_t ch, std::size_t pos = 0) const noexcept;

    // Conversion
    [[nodiscard]] std::string to_utf8() const;
    [[nodiscard]] std::u16string to_utf16() const;
    [[nodiscard]] std::u32string to_utf32() const;

    // Character classification (Unicode-aware)
    [[nodiscard]] static bool is_whitespace(char32_t ch) noexcept;
    [[nodiscard]] static bool is_alphanumeric(char32_t ch) noexcept;
    [[nodiscard]] static bool is_printable(char32_t ch) noexcept;

    // SIMD-optimized operations
    [[nodiscard]] bool contains_simd(char32_t ch) const noexcept;
    [[nodiscard]] std::size_t count_simd(char32_t ch) const noexcept;
    void replace_simd(char32_t from, char32_t to) noexcept;
};
} // namespace text

/**
 * @brief Modern text line with efficient storage and operations
 */
class TextLine {
  private:
    text::UnicodeString content_;
    std::size_t line_number_{0};
    bool modified_{false};

    // Visual representation cache
    mutable std::optional<std::vector<ScreenCoordinate>> visual_positions_;
    mutable std::optional<std::int32_t> display_width_;

  public:
    TextLine() = default;
    explicit TextLine(text::UnicodeString content, std::size_t line_num = 0);
    explicit TextLine(std::string_view content, std::size_t line_num = 0);

    // Content access
    [[nodiscard]] const text::UnicodeString &content() const noexcept { return content_; }
    [[nodiscard]] text::UnicodeString &content() noexcept {
        modified_ = true;
        visual_positions_.reset();
        display_width_.reset();
        return content_;
    }

    [[nodiscard]] std::size_t length() const noexcept { return content_.length(); }
    [[nodiscard]] bool empty() const noexcept { return content_.empty(); }
    [[nodiscard]] std::size_t line_number() const noexcept { return line_number_; }
    [[nodiscard]] bool is_modified() const noexcept { return modified_; }

    // Character access
    [[nodiscard]] char32_t at(std::size_t pos) const { return content_.at(pos); }
    [[nodiscard]] char32_t operator[](std::size_t pos) const { return content_[pos]; }

    // Modification
    void insert(std::size_t pos, char32_t ch);
    void insert(std::size_t pos, const text::UnicodeString &str);
    void erase(std::size_t pos, std::size_t count = 1);
    void append(char32_t ch);
    void append(const text::UnicodeString &str);
    void clear();

    // Visual representation
    [[nodiscard]] std::int32_t display_width(std::int32_t tab_size = 8) const;
    [[nodiscard]] std::size_t column_to_position(std::int32_t column,
                                                 std::int32_t tab_size = 8) const;
    [[nodiscard]] std::int32_t position_to_column(std::size_t pos, std::int32_t tab_size = 8) const;

    // Line operations
    [[nodiscard]] std::pair<TextLine, TextLine> split(std::size_t pos) const;
    [[nodiscard]] TextLine merge(const TextLine &other) const;

    // Search operations (SIMD-optimized)
    [[nodiscard]] std::vector<std::size_t> find_all(char32_t ch) const;
    [[nodiscard]] std::vector<std::size_t> find_all(const text::UnicodeString &pattern) const;
    [[nodiscard]] std::optional<std::size_t> find_regex(const std::u32string &pattern) const;

    // Utility
    void set_line_number(std::size_t num) noexcept { line_number_ = num; }
    void mark_clean() noexcept { modified_ = false; }

    // Comparison
    auto operator==(const TextLine &other) const -> bool;
};

/**
 * @brief Efficient text buffer with modern C++ features
 */
class TextBuffer {
  public:
    using LineContainer = std::deque<std::unique_ptr<TextLine>>;
    using Iterator = LineContainer::iterator;
    using ConstIterator = LineContainer::const_iterator;

  private:
    LineContainer lines_;
    std::filesystem::path file_path_;
    bool modified_{false};
    bool read_only_{false};
    text::Encoding encoding_{text::Encoding::UTF8};

    // Undo/Redo system
    struct EditOperation {
        enum class Type { Insert, Delete, Replace };
        Type type;
        Position position;
        text::UnicodeString old_text;
        text::UnicodeString new_text;

        EditOperation(Type t, Position pos, text::UnicodeString old_text = {},
                      text::UnicodeString new_text = {})
            : type(t), position(pos), old_text(std::move(old_text)), new_text(std::move(new_text)) {
        }

        virtual ~EditOperation() = default;
    };
    std::vector<std::unique_ptr<EditOperation>> undo_stack_;
    std::vector<std::unique_ptr<EditOperation>> redo_stack_;
    std::size_t max_undo_levels_{1000};

    // Indexing for fast access
    mutable std::optional<std::vector<std::size_t>> line_offsets_;

  public:
    TextBuffer();
    explicit TextBuffer(const std::filesystem::path &file_path);
    ~TextBuffer() = default;

    // Disable copy, enable move
    TextBuffer(const TextBuffer &) = delete;
    TextBuffer &operator=(const TextBuffer &) = delete;
    TextBuffer(TextBuffer &&) = default;
    TextBuffer &operator=(TextBuffer &&) = default;

    // File operations
    [[nodiscard]] Result<void> load_from_file(const std::filesystem::path &path);
    [[nodiscard]] Result<void> save_to_file() const;
    [[nodiscard]] Result<void> save_to_file(const std::filesystem::path &path) const;
    [[nodiscard]] Result<void> reload_from_file();

    // Buffer properties
    [[nodiscard]] std::size_t line_count() const noexcept { return lines_.size(); }
    [[nodiscard]] bool empty() const noexcept { return lines_.empty(); }
    [[nodiscard]] bool is_modified() const noexcept { return modified_; }
    [[nodiscard]] bool is_read_only() const noexcept { return read_only_; }
    [[nodiscard]] const std::filesystem::path &file_path() const noexcept { return file_path_; }
    [[nodiscard]] text::Encoding encoding() const noexcept { return encoding_; }

    // Line access
    [[nodiscard]] TextLine &line_at(std::size_t index);
    [[nodiscard]] const TextLine &line_at(std::size_t index) const;
    [[nodiscard]] TextLine &operator[](std::size_t index) { return line_at(index); }
    [[nodiscard]] const TextLine &operator[](std::size_t index) const { return line_at(index); }

    // Iterators
    [[nodiscard]] Iterator begin() noexcept { return lines_.begin(); }
    [[nodiscard]] Iterator end() noexcept { return lines_.end(); }
    [[nodiscard]] ConstIterator begin() const noexcept { return lines_.begin(); }
    [[nodiscard]] ConstIterator end() const noexcept { return lines_.end(); }
    [[nodiscard]] ConstIterator cbegin() const noexcept { return lines_.cbegin(); }
    [[nodiscard]] ConstIterator cend() const noexcept { return lines_.cend(); }

    // Text modification
    void insert_character(Position pos, char32_t ch);
    void insert_text(Position pos, const text::UnicodeString &text);
    void insert_line(std::size_t line_index, std::unique_ptr<TextLine> line);
    void erase_character(Position pos);
    void erase_text(Position start, Position end);
    void erase_line(std::size_t line_index);

    // Line operations
    void split_line(Position pos);
    void merge_lines(std::size_t line1, std::size_t line2);
    void move_line(std::size_t from, std::size_t to);

    // Search and replace (with async support)
    [[nodiscard]] std::vector<Position> find_all(const text::UnicodeString &pattern) const;
    [[nodiscard]] std::vector<Position> find_regex(const std::u32string &pattern) const;
    [[nodiscard]] std::future<std::size_t>
    replace_all_async(const text::UnicodeString &find_pattern,
                      const text::UnicodeString &replace_pattern);

    // Undo/Redo
    void undo();
    void redo();
    [[nodiscard]] bool can_undo() const noexcept { return !undo_stack_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_stack_.empty(); }
    void clear_undo_history();

    // Utility
    [[nodiscard]] std::size_t total_character_count() const;
    [[nodiscard]] Position end_position() const;
    [[nodiscard]] bool is_valid_position(Position pos) const noexcept;
    void mark_clean() noexcept;

    // Statistics
    struct BufferStats {
        std::size_t line_count;
        std::size_t character_count;
        std::size_t word_count;
        std::size_t byte_size;
        bool has_bom;
        text::Encoding encoding;
    };
    [[nodiscard]] BufferStats get_statistics() const;
};

/**
 * @brief Smart cursor with movement validation and history
 */
class Cursor {
  private:
    Position position_{0, 0};
    Position preferred_column_{}; // For vertical movement
    std::vector<Position> position_history_;
    std::size_t history_index_{0};
    const TextBuffer *buffer_{nullptr};

  public:
    explicit Cursor(const TextBuffer *buffer = nullptr) : buffer_(buffer) {}

    // Position management
    [[nodiscard]] Position position() const noexcept { return position_; }
    [[nodiscard]] std::size_t line() const noexcept { return position_.line; }
    [[nodiscard]] std::size_t column() const noexcept { return position_.column; }

    void set_position(Position pos);
    void set_buffer(const TextBuffer *buffer) noexcept { buffer_ = buffer; }

    // Movement operations
    void move_up(std::size_t count = 1);
    void move_down(std::size_t count = 1);
    void move_left(std::size_t count = 1);
    void move_right(std::size_t count = 1);
    void move_to_line_start();
    void move_to_line_end();
    void move_to_buffer_start();
    void move_to_buffer_end();
    void move_to_word_start();
    void move_to_word_end();
    void move_to_next_word();
    void move_to_previous_word();

    // Navigation history
    void save_position();
    void goto_previous_position();
    void goto_next_position();
    [[nodiscard]] bool can_go_back() const noexcept { return history_index_ > 0; }
    [[nodiscard]] bool can_go_forward() const noexcept {
        return history_index_ < position_history_.size() - 1;
    }

    // Position validation
    [[nodiscard]] bool is_valid() const;
    [[nodiscard]] Position clamp_to_buffer() const;

  private:
    void add_to_history(Position pos);
    void ensure_valid_position();
};

/**
 * @brief Modern display system with efficient rendering
 */
class Display {
  public:
    enum class Color : std::uint8_t {
        Black = 0,
        Red,
        Green,
        Yellow,
        Blue,
        Magenta,
        Cyan,
        White,
        BrightBlack,
        BrightRed,
        BrightGreen,
        BrightYellow,
        BrightBlue,
        BrightMagenta,
        BrightCyan,
        BrightWhite
    };

    enum class Style : std::uint8_t {
        Normal = 0,
        Bold = 1,
        Dim = 2,
        Italic = 3,
        Underline = 4,
        Reverse = 7,
        Strikethrough = 9
    };

    struct TextAttributes {
        Color foreground{Color::White};
        Color background{Color::Black};
        std::uint8_t styles{0}; // Bitfield of Style values

        constexpr auto operator<=>(const TextAttributes &) const = default;
    };

  private:
    DisplayRegion region_{};
    std::vector<std::vector<char32_t>> screen_buffer_;
    std::vector<std::vector<TextAttributes>> attribute_buffer_;
    ScreenCoordinate cursor_position_{0, 0};
    bool cursor_visible_{true};
    std::int32_t tab_size_{8};

    // Rendering optimization
    mutable std::vector<std::pair<std::int32_t, std::int32_t>> dirty_regions_;
    mutable bool full_redraw_needed_{true};

  public:
    explicit Display(DisplayRegion region = {{0, 0}, {79, 23}});

    // Display properties
    [[nodiscard]] DisplayRegion region() const noexcept { return region_; }
    [[nodiscard]] std::int32_t width() const noexcept { return region_.width(); }
    [[nodiscard]] std::int32_t height() const noexcept { return region_.height(); }
    [[nodiscard]] std::int32_t tab_size() const noexcept { return tab_size_; }
    void set_tab_size(std::int32_t size) noexcept { tab_size_ = size; }

    // Cursor management
    [[nodiscard]] ScreenCoordinate cursor_position() const noexcept { return cursor_position_; }
    [[nodiscard]] bool is_cursor_visible() const noexcept { return cursor_visible_; }
    void set_cursor_position(ScreenCoordinate pos);
    void set_cursor_visible(bool visible) noexcept { cursor_visible_ = visible; }

    // Text rendering
    void clear();
    void clear_line(std::int32_t y);
    void put_char(ScreenCoordinate pos, char32_t ch, TextAttributes attr);
    void put_text(ScreenCoordinate pos, const text::UnicodeString &text, TextAttributes attr);
    void put_line(std::int32_t y, const TextLine &line, TextAttributes attr);

    // Buffer rendering
    void render_buffer(const TextBuffer &buffer, std::size_t start_line, const Cursor &cursor,
                       std::int32_t scroll_offset = 0);

    // Status line
    void show_status(const std::string &message,
                     TextAttributes attr = {Color::Black, Color::White, 0});
    void show_error(const std::string &error_message);
    void clear_status();

    // Drawing primitives
    void draw_box(ScreenCoordinate top_left, ScreenCoordinate bottom_right, TextAttributes attr);
    void draw_horizontal_line(std::int32_t y, std::int32_t start_x, std::int32_t end_x, char32_t ch,
                              TextAttributes attr);
    void draw_vertical_line(std::int32_t x, std::int32_t start_y, std::int32_t end_y, char32_t ch,
                            TextAttributes attr);

    // Scrolling
    void scroll_up(std::int32_t lines = 1);
    void scroll_down(std::int32_t lines = 1);

    // Refresh and optimization
    void refresh();
    void force_full_redraw() { full_redraw_needed_ = true; }
    void mark_dirty(ScreenCoordinate top_left, ScreenCoordinate bottom_right);

  private:
    void ensure_screen_size();
    void output_escape_sequence(std::string_view sequence);
    void set_text_attributes(TextAttributes attr);
    std::string attributes_to_ansi(TextAttributes attr) const;
};

/**
 * @brief Command system with coroutine support
 */
namespace commands {
enum class CommandType : std::uint32_t {
    // Movement commands
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    MoveLineStart,
    MoveLineEnd,
    MoveBufferStart,
    MoveBufferEnd,
    MoveWordForward,
    MoveWordBackward,
    MovePageUp,
    MovePageDown,
    GoToLine,
    GoToPosition,

    // Editing commands
    InsertCharacter,
    InsertText,
    InsertLine,
    DeleteCharacter,
    DeleteWord,
    DeleteLine,
    DeleteToEnd,
    Backspace,
    BackspaceWord,

    // Selection and clipboard
    SelectAll,
    SelectLine,
    SelectWord,
    CopySelection,
    CutSelection,
    Paste,

    // File operations
    NewFile,
    OpenFile,
    SaveFile,
    SaveAs,
    CloseFile,
    Quit,

    // Search and replace
    Find,
    FindNext,
    FindPrevious,
    Replace,
    ReplaceAll,

    // Undo/Redo
    Undo,
    Redo,

    // View commands
    ScrollUp,
    ScrollDown,
    Refresh,
    ToggleCursor,

    // Editor commands
    ShowStatus,
    ShowHelp,
    ShowStatistics
};

struct CommandContext {
    TextBuffer *buffer{nullptr};
    Cursor *cursor{nullptr};
    Display *display{nullptr};
    std::string argument;
    std::int32_t repeat_count{1};
    bool is_interactive{true};
};

class Command {
  public:
    virtual ~Command() = default;
    virtual Result<void> execute(CommandContext &context) = 0;
    virtual Result<void> undo(CommandContext &context) {
        return std::unexpected(ResultCode::InvalidOperation);
    }
    virtual bool is_undoable() const noexcept { return false; }
    virtual std::string description() const = 0;
};

template <CommandType Type> class SpecificCommand : public Command {
  public:
    Result<void> execute(CommandContext &context) override;
    std::string description() const override;
};

class CommandRegistry {
  private:
    std::unordered_map<CommandType, std::unique_ptr<Command>> commands_;
    std::unordered_map<std::int32_t, CommandType> key_bindings_;

  public:
    CommandRegistry();

    void register_command(CommandType type, std::unique_ptr<Command> command);
    void bind_key(std::int32_t key, CommandType command);

    [[nodiscard]] Result<void> execute_command(CommandType type, CommandContext &context);
    [[nodiscard]] Result<void> execute_key(std::int32_t key, CommandContext &context);

    [[nodiscard]] const Command *get_command(CommandType type) const;
    [[nodiscard]] std::optional<CommandType> get_command_for_key(std::int32_t key) const;

    // Default key bindings
    void setup_default_bindings();
};
} // namespace commands

/**
 * @brief Async input system with modern event handling
 */
namespace input {
enum class KeyCode : std::int32_t {
    // Special keys
    Unknown = -1,
    Escape = 27,
    Enter = 13,
    Tab = 9,
    Backspace = 8,
    Delete = 127,

    // Arrow keys
    ArrowUp = 1000,
    ArrowDown,
    ArrowLeft,
    ArrowRight,

    // Function keys
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,

    // Page navigation
    PageUp,
    PageDown,
    Home,
    End,

    // Modifiers
    CtrlA = 1,
    CtrlB,
    CtrlC,
    CtrlD,
    CtrlE,
    CtrlF,
    CtrlG,
    CtrlH,
    CtrlI,
    CtrlJ,
    CtrlK,
    CtrlL,
    CtrlM,
    CtrlN,
    CtrlO,
    CtrlP,
    CtrlQ,
    CtrlR,
    CtrlS,
    CtrlT,
    CtrlU,
    CtrlV,
    CtrlW,
    CtrlX,
    CtrlY,
    CtrlZ
};

struct KeyEvent {
    KeyCode key;
    char32_t character{0};
    bool ctrl{false};
    bool alt{false};
    bool shift{false};
    std::chrono::steady_clock::time_point timestamp;
};

class InputSystem {
  private:
    std::atomic<bool> running_{false};
    std::jthread input_thread_;
    std::mutex event_queue_mutex_;
    std::queue<KeyEvent> event_queue_;
    std::condition_variable event_available_;

  public:
    InputSystem();
    ~InputSystem();

    void start();
    void stop();

    [[nodiscard]] std::optional<KeyEvent>
    get_next_event(std::chrono::milliseconds timeout = std::chrono::milliseconds{100});
    [[nodiscard]] bool has_pending_events() const;
    void clear_event_queue();

  private:
    void input_thread_main();
    KeyEvent read_key();
    void setup_terminal();
    void restore_terminal();
};
} // namespace input

/**
 * @brief Main editor engine with coroutine-based event loop
 */
class EditorEngine {
  private:
    std::unique_ptr<TextBuffer> buffer_;
    std::unique_ptr<Cursor> cursor_;
    std::unique_ptr<Display> display_;
    std::unique_ptr<commands::CommandRegistry> command_registry_;
    std::unique_ptr<input::InputSystem> input_system_;

    std::atomic<bool> running_{false};
    std::atomic<bool> needs_redraw_{true};

    // Editor state
    bool show_line_numbers_{true};
    bool auto_indent_{true};
    std::int32_t tab_size_{4};
    bool insert_mode_{true};

    // Status information
    std::string status_message_;
    std::chrono::steady_clock::time_point status_timeout_;

  public:
    EditorEngine();
    ~EditorEngine();

    // Main interface
    [[nodiscard]] Result<void> initialize();
    [[nodiscard]] Result<void> run();
    void shutdown();

    // File operations
    [[nodiscard]] Result<void> open_file(const std::filesystem::path &path);
    [[nodiscard]] Result<void> save_file();
    [[nodiscard]] Result<void> save_file_as(const std::filesystem::path &path);
    [[nodiscard]] Result<void> new_file();

    // Editor configuration
    void set_show_line_numbers(bool show) noexcept {
        show_line_numbers_ = show;
        needs_redraw_ = true;
    }
    void set_auto_indent(bool auto_indent) noexcept { auto_indent_ = auto_indent; }
    void set_tab_size(std::int32_t size) noexcept {
        tab_size_ = size;
        display_->set_tab_size(size);
    }
    void set_insert_mode(bool insert) noexcept { insert_mode_ = insert; }

    [[nodiscard]] bool show_line_numbers() const noexcept { return show_line_numbers_; }
    [[nodiscard]] bool auto_indent() const noexcept { return auto_indent_; }
    [[nodiscard]] std::int32_t tab_size() const noexcept { return tab_size_; }
    [[nodiscard]] bool insert_mode() const noexcept { return insert_mode_; }

    // Status and UI
    void show_status(const std::string &message,
                     std::chrono::milliseconds duration = std::chrono::seconds{3});
    void show_error(const std::string &error);
    void clear_status();

    // Command execution
    [[nodiscard]] Result<void> execute_command(commands::CommandType command,
                                               const std::string &argument = "");

    // Accessors for command system
    [[nodiscard]] TextBuffer *buffer() noexcept { return buffer_.get(); }
    [[nodiscard]] const TextBuffer *buffer() const noexcept { return buffer_.get(); }
    [[nodiscard]] Cursor *cursor() noexcept { return cursor_.get(); }
    [[nodiscard]] const Cursor *cursor() const noexcept { return cursor_.get(); }
    [[nodiscard]] Display *display() noexcept { return display_.get(); }
    [[nodiscard]] const Display *display() const noexcept { return display_.get(); }

  private:
    // Main event loop
    Task<void> main_loop();
    void process_input();
    void update_display();
    void handle_resize();

    // Command creation helpers
    commands::CommandContext create_command_context(const std::string &argument = "");

    // Utility
    void setup_default_configuration();
    std::string format_status_line() const;
};

/**
 * @brief Factory functions and utilities
 */
namespace factory {
[[nodiscard]] std::unique_ptr<EditorEngine> create_editor();
[[nodiscard]] std::unique_ptr<TextBuffer> create_buffer();
[[nodiscard]] std::unique_ptr<TextBuffer>
create_buffer_from_file(const std::filesystem::path &path);

// Configuration
struct EditorConfig {
    bool show_line_numbers{true};
    bool auto_indent{true};
    std::int32_t tab_size{4};
    bool insert_mode{true};
    DisplayRegion display_region{{0, 0}, {79, 23}};
    text::Encoding default_encoding{text::Encoding::UTF8};
};

[[nodiscard]] std::unique_ptr<EditorEngine> create_configured_editor(const EditorConfig &config);
} // namespace factory

/**
 * @brief Statistics and profiling
 */
namespace profiling {
struct PerformanceStats {
    std::size_t total_commands_executed{0};
    std::size_t total_characters_processed{0};
    std::chrono::microseconds total_rendering_time{0};
    std::chrono::microseconds total_input_processing_time{0};
    std::chrono::steady_clock::time_point session_start;

    [[nodiscard]] double commands_per_second() const;
    [[nodiscard]] double characters_per_second() const;
    [[nodiscard]] std::chrono::duration<double> session_duration() const;
};

class Profiler {
  private:
    PerformanceStats stats_;
    bool enabled_{false};

  public:
    void enable() noexcept { enabled_ = true; }
    void disable() noexcept { enabled_ = false; }
    [[nodiscard]] bool is_enabled() const noexcept { return enabled_; }

    void record_command_execution();
    void record_characters_processed(std::size_t count);
    void record_rendering_time(std::chrono::microseconds time);
    void record_input_processing_time(std::chrono::microseconds time);

    [[nodiscard]] const PerformanceStats &get_stats() const noexcept { return stats_; }
    void reset_stats();

    void print_report() const;
};

extern Profiler global_profiler;
} // namespace profiling

} // namespace xinim::editor
