#pragma once

#include <array>
#include <string_view>

namespace mined::modern::command {

/// Names of movement/navigation commands.
inline constexpr std::array<std::string_view, 10> movement_names{
    "move_up",       "move_down",         "move_left",          "move_right", "move_line_start",
    "move_line_end", "move_word_forward", "move_word_backward", "page_up",    "page_down"};

/// Names of editing commands.
inline constexpr std::array<std::string_view, 4> editing_names{
    "insert_char", "insert_newline", "delete_char", "delete_char_backward"};

/// Names of file/system commands.
inline constexpr std::array<std::string_view, 2> system_names{"save", "quit"};

/// Names of history commands.
inline constexpr std::array<std::string_view, 2> history_names{"undo", "redo"};

} // namespace mined::modern::command
