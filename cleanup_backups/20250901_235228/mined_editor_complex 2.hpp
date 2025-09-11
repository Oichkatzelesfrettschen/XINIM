/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/**
 * @file mined_editor.hpp
 * @brief Main editor class that integrates all modernized components
 */

#pragma once

#include "mined.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <optional>
#include <filesystem>

namespace xinim::editor {

// Forward declarations for classes that will be implemented
class CommandDispatcher;
class EventQueue;
class UndoManager;
class SearchEngine;  
class FileManager;
class Profiler;
class Viewport;
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace mined::modern {

/**
 * @brief Main editor class that orchestrates all components
 *
 * This class provides the high-level interface for the modernized MINED editor,
 * integrating the text buffer, cursor, display, command system, and other components
 * into a cohesive editor experience.
 */
class ModernMinedEditor {
  public:
    /**
     * @brief Editor configuration
     */
    struct Config {
        std::size_t display_width = 80;
        std::size_t display_height = 24;
        std::size_t undo_history_size = 1000;
        std::size_t tab_width = 8;
        bool auto_save = false;
        std::chrono::minutes auto_save_interval{5};
        std::filesystem::path backup_directory = ".mined_backups";
    };

    /**
     * @brief Editor status information
     */
    struct Status {
        std::filesystem::path current_file;
        bool is_modified = false;
        bool is_read_only = false;
        std::size_t total_lines = 0;
        std::size_t current_line = 0;
        std::size_t current_column = 0;
        std::string last_command;
        std::string status_message;
        std::chrono::system_clock::time_point last_save_time;
    };

    /**
     * @brief Initialize editor with configuration
     */
    explicit ModernMinedEditor(Config config = {});

    /**
     * @brief Destructor - ensures cleanup
     */
    ~ModernMinedEditor();

    // Non-copyable, movable
    ModernMinedEditor(const ModernMinedEditor &) = delete;
    ModernMinedEditor &operator=(const ModernMinedEditor &) = delete;
    ModernMinedEditor(ModernMinedEditor &&) = default;
    ModernMinedEditor &operator=(ModernMinedEditor &&) = default;

    /**
     * @brief Start the editor main loop
     */
    auto run() -> xinim::editor::Result<void>;

    /**
     * @brief Load a file into the editor
     */
    auto load_file(const std::filesystem::path &path) -> xinim::editor::Result<void>;

    /**
     * @brief Save current buffer to file
     */
    auto save_file(std::optional<std::filesystem::path> path = std::nullopt) -> xinim::editor::Result<void>;

    /**
     * @brief Create a new empty buffer
     */
    auto new_file() -> xinim::editor::Result<void>;

    /**
     * @brief Execute a command by name
     */
    auto execute_command(std::string_view command, std::string_view args = "") -> xinim::editor::Result<void>;

    /**
     * @brief Get current editor status
     */
    auto get_status() const -> Status;

    /**
     * @brief Check if editor has unsaved changes
     */
    bool has_unsaved_changes() const;

    /**
     * @brief Quit the editor
     */
    auto quit(bool force = false) -> xinim::editor::Result<void>;

  private:
    // Core components
    Config config_;
    std::unique_ptr<xinim::editor::TextBuffer> buffer_;
    std::unique_ptr<xinim::editor::Cursor> cursor_;
    std::unique_ptr<xinim::editor::Display> display_;
    std::unique_ptr<CommandDispatcher> command_dispatcher_;
    std::unique_ptr<EventQueue> event_queue_;
    std::unique_ptr<UndoManager> undo_manager_;
    std::unique_ptr<SearchEngine> search_engine_;
    std::unique_ptr<FileManager> file_manager_;
    std::unique_ptr<Profiler> profiler_;

    // State
    mutable std::mutex state_mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_quit_{false};
    Status status_;
    Viewport viewport_;
    std::optional<xinim::editor::Position> mark_position_;
    xinim::editor::text::UnicodeString clipboard_;

    // Threads
    std::unique_ptr<std::thread> input_thread_;
    std::unique_ptr<std::thread> auto_save_thread_;

    // Private methods
    auto initialize_components() -> xinim::editor::Result<void>;
    auto setup_commands() -> xinim::editor::Result<void>;
    auto setup_input_handling() -> xinim::editor::Result<void>;
    auto main_loop() -> xinim::editor::Result<void>;
    auto handle_event(const Event &event) -> xinim::editor::Result<void>;
    auto handle_key_event(const KeyEvent &key_event) -> xinim::editor::Result<void>;
    auto update_display() -> xinim::editor::Result<void>;
    auto update_viewport() -> xinim::editor::Result<void>;
    auto update_status() -> xinim::editor::Result<void>;
    auto handle_input_thread();
    auto handle_auto_save_thread();

    // Command implementations
    auto cmd_move_up(const CommandContext &ctx) -> Result<void>;
    auto cmd_move_down(const CommandContext &ctx) -> Result<void>;
    auto cmd_move_left(const CommandContext &ctx) -> Result<void>;
    auto cmd_move_right(const CommandContext &ctx) -> Result<void>;
    auto cmd_move_line_start(const CommandContext &ctx) -> Result<void>;
    auto cmd_move_line_end(const CommandContext &ctx) -> Result<void>;
    auto cmd_move_word_forward(const CommandContext &ctx) -> Result<void>;
    auto cmd_move_word_backward(const CommandContext &ctx) -> Result<void>;
    auto cmd_page_up(const CommandContext &ctx) -> Result<void>;
    auto cmd_page_down(const CommandContext &ctx) -> Result<void>;
    auto cmd_goto_line(const CommandContext &ctx) -> Result<void>;

    auto cmd_insert_char(const CommandContext &ctx) -> Result<void>;
    auto cmd_insert_newline(const CommandContext &ctx) -> Result<void>;
    auto cmd_delete_char(const CommandContext &ctx) -> Result<void>;
    auto cmd_delete_char_backward(const CommandContext &ctx) -> Result<void>;
    auto cmd_delete_word(const CommandContext &ctx) -> Result<void>;
    auto cmd_delete_line(const CommandContext &ctx) -> Result<void>;

    auto cmd_undo(const CommandContext &ctx) -> Result<void>;
    auto cmd_redo(const CommandContext &ctx) -> Result<void>;
    auto cmd_copy(const CommandContext &ctx) -> Result<void>;
    auto cmd_cut(const CommandContext &ctx) -> Result<void>;
    auto cmd_paste(const CommandContext &ctx) -> Result<void>;
    auto cmd_select_all(const CommandContext &ctx) -> Result<void>;
    auto cmd_set_mark(const CommandContext &ctx) -> Result<void>;

    auto cmd_search_forward(const CommandContext &ctx) -> Result<void>;
    auto cmd_search_backward(const CommandContext &ctx) -> Result<void>;
    auto cmd_replace(const CommandContext &ctx) -> Result<void>;
    auto cmd_replace_all(const CommandContext &ctx) -> Result<void>;

    auto cmd_save(const CommandContext &ctx) -> Result<void>;
    auto cmd_save_as(const CommandContext &ctx) -> Result<void>;
    auto cmd_load(const CommandContext &ctx) -> Result<void>;
    auto cmd_new(const CommandContext &ctx) -> Result<void>;
    auto cmd_quit(const CommandContext &ctx) -> Result<void>;
    auto cmd_force_quit(const CommandContext &ctx) -> Result<void>;

    // Utility methods
    auto ensure_cursor_visible() -> Result<void>;
    auto get_selection_range() const -> std::optional<std::pair<Position, Position>>;
    auto create_backup() -> Result<void>;
    auto prompt_for_input(std::string_view prompt) -> Result<std::string>;
    auto show_message(std::string_view message) -> void;
    auto show_error(std::string_view error) -> void;
};

/**
 * @brief Factory function to create and configure the editor
 */
auto create_editor(ModernMinedEditor::Config config = {}) -> std::unique_ptr<ModernMinedEditor>;

/**
 * @brief Main entry point for the modernized MINED editor
 */
auto main_editor(int argc, char *argv[]) -> int;

} // namespace mined::modern
