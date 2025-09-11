/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/**
 * @file mined_editor.cpp
 * @brief Implementation of the main modernized MINED editor class
 */

#include "mined_editor.hpp"
#include <algorithm>
#include <format>
#include <iostream>
#include <sstream>

#ifdef __unix__
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace mined::modern {

// =============================================================================
// ModernMinedEditor Implementation
// =============================================================================

ModernMinedEditor::ModernMinedEditor(Config config) : config_(std::move(config)), viewport_{0, 0} {
    auto init_result = initialize_components();
    if (!init_result) {
        throw std::runtime_error(
            std::format("Failed to initialize editor: {}", init_result.error().message));
    }
}

ModernMinedEditor::~ModernMinedEditor() {
    should_quit_ = true;

    if (input_thread_ && input_thread_->joinable()) {
        input_thread_->join();
    }

    if (auto_save_thread_ && auto_save_thread_->joinable()) {
        auto_save_thread_->join();
    }
}

auto ModernMinedEditor::initialize_components() -> Result<void> {
    try {
        // Initialize core components
        buffer_ = std::make_unique<TextBuffer>();
        cursor_ = std::make_unique<Cursor>(*buffer_);
        display_ = std::make_unique<Display>(config_.display_width, config_.display_height);
        command_dispatcher_ = std::make_unique<CommandDispatcher>();
        event_queue_ = std::make_unique<EventQueue>();
        undo_manager_ = std::make_unique<UndoManager>(config_.undo_history_size);
        search_engine_ = std::make_unique<SearchEngine>();
        file_manager_ = std::make_unique<FileManager>();
        profiler_ = std::make_unique<Profiler>();

        // Setup commands and input handling
        auto setup_result = setup_commands();
        if (!setup_result) {
            return std::unexpected(setup_result.error());
        }

        setup_result = setup_input_handling();
        if (!setup_result) {
            return std::unexpected(setup_result.error());
        }

        // Initialize status
        status_.total_lines = buffer_->line_count();
        status_.current_line = cursor_->position().line;
        status_.current_column = cursor_->position().column;

        return {};
    } catch (const std::exception &e) {
        return std::unexpected(EditorError{
            ErrorType::SystemError, std::format("Component initialization failed: {}", e.what())});
    }
}

auto ModernMinedEditor::setup_commands() -> Result<void> {
    // Movement commands
    auto register_result = command_dispatcher_->register_command(
        "move_up", [this](const CommandContext &ctx) { return cmd_move_up(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "move_down", [this](const CommandContext &ctx) { return cmd_move_down(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "move_left", [this](const CommandContext &ctx) { return cmd_move_left(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "move_right", [this](const CommandContext &ctx) { return cmd_move_right(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "move_line_start", [this](const CommandContext &ctx) { return cmd_move_line_start(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "move_line_end", [this](const CommandContext &ctx) { return cmd_move_line_end(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "move_word_forward",
        [this](const CommandContext &ctx) { return cmd_move_word_forward(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "move_word_backward",
        [this](const CommandContext &ctx) { return cmd_move_word_backward(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "page_up", [this](const CommandContext &ctx) { return cmd_page_up(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "page_down", [this](const CommandContext &ctx) { return cmd_page_down(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    // Editing commands
    register_result = command_dispatcher_->register_command(
        "insert_char", [this](const CommandContext &ctx) { return cmd_insert_char(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "insert_newline", [this](const CommandContext &ctx) { return cmd_insert_newline(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "delete_char", [this](const CommandContext &ctx) { return cmd_delete_char(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "delete_char_backward",
        [this](const CommandContext &ctx) { return cmd_delete_char_backward(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    // File operations
    register_result = command_dispatcher_->register_command(
        "save", [this](const CommandContext &ctx) { return cmd_save(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "quit", [this](const CommandContext &ctx) { return cmd_quit(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    // Undo/Redo
    register_result = command_dispatcher_->register_command(
        "undo", [this](const CommandContext &ctx) { return cmd_undo(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    register_result = command_dispatcher_->register_command(
        "redo", [this](const CommandContext &ctx) { return cmd_redo(ctx); });
    if (!register_result)
        return std::unexpected(register_result.error());

    return {};
}

auto ModernMinedEditor::setup_input_handling() -> Result<void> {
    try {
        input_thread_ = std::make_unique<std::thread>([this] { handle_input_thread(); });

        if (config_.auto_save) {
            auto_save_thread_ =
                std::make_unique<std::thread>([this] { handle_auto_save_thread(); });
        }

        return {};
    } catch (const std::exception &e) {
        return std::unexpected(EditorError{
            ErrorType::SystemError, std::format("Failed to setup input handling: {}", e.what())});
    }
}

auto ModernMinedEditor::run() -> Result<void> {
    running_ = true;

    try {
        // Initial display update
        auto update_result = update_display();
        if (!update_result) {
            return std::unexpected(update_result.error());
        }

        return main_loop();
    } catch (const std::exception &e) {
        return std::unexpected(
            EditorError{ErrorType::SystemError, std::format("Editor run failed: {}", e.what())});
    }
}

auto ModernMinedEditor::main_loop() -> Result<void> {
    while (running_ && !should_quit_) {
        // Process events with timeout
        auto event_result = event_queue_->pop_event(std::chrono::milliseconds(100));

        if (event_result) {
            auto handle_result = handle_event(event_result.value());
            if (!handle_result) {
                show_error(handle_result.error().message);
            }

            // Update display after processing event
            auto update_result = update_display();
            if (!update_result) {
                return std::unexpected(update_result.error());
            }
        }

        // Update status periodically
        auto status_result = update_status();
        if (!status_result) {
            return std::unexpected(status_result.error());
        }
    }

    return {};
}

auto ModernMinedEditor::handle_event(const Event &event) -> Result<void> {
    return std::visit(
        [this](const auto &e) -> Result<void> {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, KeyEvent>) {
                return handle_key_event(e);
            } else if constexpr (std::is_same_v<T, CommandEvent>) {
                CommandContext ctx;
                ctx.args = e.args;
                return command_dispatcher_->execute_command(e.command, ctx);
            } else if constexpr (std::is_same_v<T, QuitEvent>) {
                should_quit_ = true;
                return {};
            } else {
                return std::unexpected(EditorError{ErrorType::UnknownEvent, "Unknown event type"});
            }
        },
        event);
}

auto ModernMinedEditor::handle_key_event(const KeyEvent &key_event) -> Result<void> {
    CommandContext ctx;

    // Map key events to commands
    switch (key_event.key) {
    case Key::Up:
        return command_dispatcher_->execute_command("move_up", ctx);
    case Key::Down:
        return command_dispatcher_->execute_command("move_down", ctx);
    case Key::Left:
        return command_dispatcher_->execute_command("move_left", ctx);
    case Key::Right:
        return command_dispatcher_->execute_command("move_right", ctx);
    case Key::Home:
        return command_dispatcher_->execute_command("move_line_start", ctx);
    case Key::End:
        return command_dispatcher_->execute_command("move_line_end", ctx);
    case Key::PageUp:
        return command_dispatcher_->execute_command("page_up", ctx);
    case Key::PageDown:
        return command_dispatcher_->execute_command("page_down", ctx);
    case Key::Enter:
        return command_dispatcher_->execute_command("insert_newline", ctx);
    case Key::Backspace:
        return command_dispatcher_->execute_command("delete_char_backward", ctx);
    case Key::Delete:
        return command_dispatcher_->execute_command("delete_char", ctx);
    case Key::Character:
        ctx.args = std::format("{}", static_cast<char32_t>(key_event.character));
        return command_dispatcher_->execute_command("insert_char", ctx);
    case Key::Ctrl_S:
        return command_dispatcher_->execute_command("save", ctx);
    case Key::Ctrl_Q:
        return command_dispatcher_->execute_command("quit", ctx);
    case Key::Ctrl_Z:
        return command_dispatcher_->execute_command("undo", ctx);
    case Key::Ctrl_Y:
        return command_dispatcher_->execute_command("redo", ctx);
    default:
        return {}; // Ignore unknown keys
    }
}

auto ModernMinedEditor::update_display() -> Result<void> {
    auto render_result = display_->render_text_buffer(*buffer_, viewport_);
    if (!render_result) {
        return std::unexpected(render_result.error());
    }

    auto cursor_result = display_->set_cursor_position(cursor_->position(), viewport_);
    if (!cursor_result) {
        return std::unexpected(cursor_result.error());
    }

    // In a real implementation, this would output to the terminal
    // For now, we just validate that the display was updated

    return {};
}

auto ModernMinedEditor::update_viewport() -> Result<void> {
    auto cursor_pos = cursor_->position();
    bool viewport_changed = false;

    // Adjust vertical viewport
    if (cursor_pos.line < viewport_.top_line) {
        viewport_.top_line = cursor_pos.line;
        viewport_changed = true;
    } else if (cursor_pos.line >= viewport_.top_line + config_.display_height) {
        viewport_.top_line = cursor_pos.line - config_.display_height + 1;
        viewport_changed = true;
    }

    // Adjust horizontal viewport
    if (cursor_pos.column < viewport_.left_offset) {
        viewport_.left_offset = cursor_pos.column;
        viewport_changed = true;
    } else if (cursor_pos.column >= viewport_.left_offset + config_.display_width) {
        viewport_.left_offset = cursor_pos.column - config_.display_width + 1;
        viewport_changed = true;
    }

    return {};
}

auto ModernMinedEditor::update_status() -> Result<void> {
    std::lock_guard<std::mutex> lock(state_mutex_);

    status_.total_lines = buffer_->line_count();
    status_.current_line = cursor_->position().line;
    status_.current_column = cursor_->position().column;

    return {};
}

auto ModernMinedEditor::load_file(const std::filesystem::path &path) -> Result<void> {
    auto load_result = file_manager_->load_file(path);
    if (!load_result) {
        return std::unexpected(load_result.error());
    }

    buffer_ = std::make_unique<TextBuffer>(std::move(load_result.value()));
    cursor_ = std::make_unique<Cursor>(*buffer_);

    std::lock_guard<std::mutex> lock(state_mutex_);
    status_.current_file = path;
    status_.is_modified = false;
    status_.last_save_time = std::chrono::system_clock::now();

    undo_manager_->clear();

    return {};
}

auto ModernMinedEditor::save_file(std::optional<std::filesystem::path> path) -> Result<void> {
    auto save_path = path.value_or(status_.current_file);

    if (save_path.empty()) {
        return std::unexpected(
            EditorError{ErrorType::FileError, "No file path specified for save"});
    }

    auto save_result = file_manager_->save_file(save_path, *buffer_);
    if (!save_result) {
        return std::unexpected(save_result.error());
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    status_.current_file = save_path;
    status_.is_modified = false;
    status_.last_save_time = std::chrono::system_clock::now();

    return {};
}

auto ModernMinedEditor::get_status() const -> Status {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return status_;
}

bool ModernMinedEditor::has_unsaved_changes() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return status_.is_modified;
}

auto ModernMinedEditor::quit(bool force) -> Result<void> {
    if (!force && has_unsaved_changes()) {
        return std::unexpected(
            EditorError{ErrorType::UnsavedChanges,
                        "Cannot quit with unsaved changes (use force=true to override)"});
    }

    should_quit_ = true;
    running_ = false;

    return {};
}

// =============================================================================
// Command Implementations
// =============================================================================

auto ModernMinedEditor::cmd_move_up(const CommandContext &ctx) -> Result<void> {
    auto move_result = cursor_->move_up(1);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return update_viewport();
}

auto ModernMinedEditor::cmd_move_down(const CommandContext &ctx) -> Result<void> {
    auto move_result = cursor_->move_down(1);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return update_viewport();
}

auto ModernMinedEditor::cmd_move_left(const CommandContext &ctx) -> Result<void> {
    auto move_result = cursor_->move_left(1);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return update_viewport();
}

auto ModernMinedEditor::cmd_move_right(const CommandContext &ctx) -> Result<void> {
    auto move_result = cursor_->move_right(1);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return update_viewport();
}

auto ModernMinedEditor::cmd_move_line_start(const CommandContext &ctx) -> Result<void> {
    auto move_result = cursor_->move_to_line_start();
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return update_viewport();
}

auto ModernMinedEditor::cmd_move_line_end(const CommandContext &ctx) -> Result<void> {
    auto move_result = cursor_->move_to_line_end();
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return update_viewport();
}

auto ModernMinedEditor::cmd_move_word_forward(const CommandContext &ctx) -> Result<void> {
    auto move_result = cursor_->move_to_word_end();
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return update_viewport();
}

auto ModernMinedEditor::cmd_move_word_backward(const CommandContext &ctx) -> Result<void> {
    auto move_result = cursor_->move_to_word_start();
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return update_viewport();
}

auto ModernMinedEditor::cmd_page_up(const CommandContext &ctx) -> Result<void> {
    auto move_result = cursor_->move_up(config_.display_height);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return update_viewport();
}

auto ModernMinedEditor::cmd_page_down(const CommandContext &ctx) -> Result<void> {
    auto move_result = cursor_->move_down(config_.display_height);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return update_viewport();
}

auto ModernMinedEditor::cmd_insert_char(const CommandContext &ctx) -> Result<void> {
    if (ctx.args.empty()) {
        return std::unexpected(
            EditorError{ErrorType::InvalidCommand, "No character specified for insertion"});
    }

    // Record undo action
    UndoAction undo_action;
    undo_action.type = UndoActionType::Insert;
    undo_action.position = cursor_->position();
    undo_action.text = ctx.args;
    undo_manager_->record_action(undo_action);

    // Parse character from args
    char32_t ch = static_cast<char32_t>(std::stoi(ctx.args));
    std::u32string text(1, ch);

    auto insert_result = buffer_->insert_text(cursor_->position(), text);
    if (!insert_result) {
        return std::unexpected(insert_result.error());
    }

    auto move_result = cursor_->move_right(1);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    status_.is_modified = true;

    return {};
}

auto ModernMinedEditor::cmd_insert_newline(const CommandContext &ctx) -> Result<void> {
    // Record undo action
    UndoAction undo_action;
    undo_action.type = UndoActionType::Insert;
    undo_action.position = cursor_->position();
    undo_action.text = U"\n";
    undo_manager_->record_action(undo_action);

    auto insert_result = buffer_->insert_text(cursor_->position(), U"\n");
    if (!insert_result) {
        return std::unexpected(insert_result.error());
    }

    auto move_result = cursor_->move_down(1);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    move_result = cursor_->move_to_line_start();
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    status_.is_modified = true;

    return {};
}

auto ModernMinedEditor::cmd_delete_char(const CommandContext &ctx) -> Result<void> {
    auto cursor_pos = cursor_->position();
    auto end_pos = cursor_pos;
    end_pos.column += 1;

    // Handle deletion at end of line
    auto line_result = buffer_->get_line(cursor_pos.line);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }

    if (cursor_pos.column >= line_result.value().content.size()) {
        // Delete newline, join with next line
        if (cursor_pos.line + 1 < buffer_->line_count()) {
            end_pos.line += 1;
            end_pos.column = 0;
        } else {
            return {}; // Nothing to delete
        }
    }

    // Record undo action
    auto text_result = buffer_->get_text_range(cursor_pos, end_pos);
    if (!text_result) {
        return std::unexpected(text_result.error());
    }

    UndoAction undo_action;
    undo_action.type = UndoActionType::Delete;
    undo_action.position = cursor_pos;
    undo_action.text = text_result.value();
    undo_manager_->record_action(undo_action);

    auto delete_result = buffer_->delete_text(cursor_pos, end_pos);
    if (!delete_result) {
        return std::unexpected(delete_result.error());
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    status_.is_modified = true;

    return {};
}

auto ModernMinedEditor::cmd_delete_char_backward(const CommandContext &ctx) -> Result<void> {
    auto cursor_pos = cursor_->position();

    if (cursor_pos.line == 0 && cursor_pos.column == 0) {
        return {}; // Nothing to delete
    }

    auto start_pos = cursor_pos;
    if (cursor_pos.column > 0) {
        start_pos.column -= 1;
    } else {
        // Delete newline from previous line
        start_pos.line -= 1;
        auto line_result = buffer_->get_line(start_pos.line);
        if (!line_result) {
            return std::unexpected(line_result.error());
        }
        start_pos.column = line_result.value().content.size();
    }

    // Record undo action
    auto text_result = buffer_->get_text_range(start_pos, cursor_pos);
    if (!text_result) {
        return std::unexpected(text_result.error());
    }

    UndoAction undo_action;
    undo_action.type = UndoActionType::Delete;
    undo_action.position = start_pos;
    undo_action.text = text_result.value();
    undo_manager_->record_action(undo_action);

    auto delete_result = buffer_->delete_text(start_pos, cursor_pos);
    if (!delete_result) {
        return std::unexpected(delete_result.error());
    }

    auto move_result = cursor_->move_to(start_pos);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    status_.is_modified = true;

    return {};
}

auto ModernMinedEditor::cmd_undo(const CommandContext &ctx) -> Result<void> {
    auto undo_result = undo_manager_->undo();
    if (!undo_result) {
        return std::unexpected(undo_result.error());
    }

    const auto &action = undo_result.value();

    switch (action.type) {
    case UndoActionType::Insert: {
        // Undo insert by deleting
        auto end_pos = action.position;
        end_pos.column += action.text.size();
        auto delete_result = buffer_->delete_text(action.position, end_pos);
        if (!delete_result) {
            return std::unexpected(delete_result.error());
        }
        break;
    }
    case UndoActionType::Delete: {
        // Undo delete by inserting
        auto insert_result = buffer_->insert_text(action.position, action.text);
        if (!insert_result) {
            return std::unexpected(insert_result.error());
        }
        break;
    }
    }

    auto move_result = cursor_->move_to(action.position);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return {};
}

auto ModernMinedEditor::cmd_redo(const CommandContext &ctx) -> Result<void> {
    auto redo_result = undo_manager_->redo();
    if (!redo_result) {
        return std::unexpected(redo_result.error());
    }

    const auto &action = redo_result.value();

    switch (action.type) {
    case UndoActionType::Insert: {
        auto insert_result = buffer_->insert_text(action.position, action.text);
        if (!insert_result) {
            return std::unexpected(insert_result.error());
        }
        break;
    }
    case UndoActionType::Delete: {
        auto end_pos = action.position;
        end_pos.column += action.text.size();
        auto delete_result = buffer_->delete_text(action.position, end_pos);
        if (!delete_result) {
            return std::unexpected(delete_result.error());
        }
        break;
    }
    }

    auto move_result = cursor_->move_to(action.position);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    return {};
}

auto ModernMinedEditor::cmd_save(const CommandContext &ctx) -> Result<void> { return save_file(); }

auto ModernMinedEditor::cmd_quit(const CommandContext &ctx) -> Result<void> { return quit(false); }

// =============================================================================
// Input Handling Threads
// =============================================================================

auto ModernMinedEditor::handle_input_thread() {
    // Simplified input handling - in a real implementation,
    // this would interface with the terminal

    while (!should_quit_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Simulate some input events for testing
        static int counter = 0;
        if (++counter > 100) { // Don't overwhelm the event queue
            counter = 0;

            KeyEvent key_event;
            key_event.key = Key::Character;
            key_event.character = U'a';

            auto push_result = event_queue_->push_event(key_event);
            if (!push_result) {
                std::cerr << "Failed to push input event: " << push_result.error().message
                          << std::endl;
            }
        }
    }
}

auto ModernMinedEditor::handle_auto_save_thread() {
    while (!should_quit_) {
        std::this_thread::sleep_for(config_.auto_save_interval);

        if (has_unsaved_changes()) {
            auto save_result = save_file();
            if (!save_result) {
                std::cerr << "Auto-save failed: " << save_result.error().message << std::endl;
            }
        }
    }
}

// =============================================================================
// Utility Methods
// =============================================================================

void ModernMinedEditor::show_message(std::string_view message) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    status_.status_message = message;
}

void ModernMinedEditor::show_error(std::string_view error) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    status_.status_message = std::format("Error: {}", error);
}

// =============================================================================
// Factory Functions
// =============================================================================

auto create_editor(ModernMinedEditor::Config config) -> std::unique_ptr<ModernMinedEditor> {
    return std::make_unique<ModernMinedEditor>(std::move(config));
}

auto main_editor(int argc, char *argv[]) -> int {
    try {
        ModernMinedEditor::Config config;

        // Parse command line arguments
        std::optional<std::filesystem::path> file_to_load;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "Modern MINED Editor\n";
                std::cout << "Usage: " << argv[0] << " [file]\n";
                std::cout << "  --help, -h    Show this help message\n";
                return 0;
            } else if (!file_to_load && !arg.starts_with("-")) {
                file_to_load = arg;
            }
        }

        auto editor = create_editor(config);

        if (file_to_load) {
            auto load_result = editor->load_file(*file_to_load);
            if (!load_result) {
                std::cerr << "Failed to load file '" << *file_to_load
                          << "': " << load_result.error().message << std::endl;
                return 1;
            }
        }

        auto run_result = editor->run();
        if (!run_result) {
            std::cerr << "Editor failed: " << run_result.error().message << std::endl;
            return 1;
        }

        return 0;

    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

} // namespace mined::modern
