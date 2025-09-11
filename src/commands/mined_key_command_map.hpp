#pragma once

#include "mined_unified.hpp"
#include <string_view>

namespace mined::modern::keymap {

/**
 * @brief Map a key press to a command name understood by the command dispatcher.
 *
 * @param key The key that was pressed.
 * @return The textual command identifier or an empty view if the key is unmapped.
 */
inline constexpr std::string_view command_for_key(xinim::mined::Key key) noexcept {
    switch (key) {
    case xinim::mined::Key::Up:
        return "move_up";
    case xinim::mined::Key::Down:
        return "move_down";
    case xinim::mined::Key::Left:
        return "move_left";
    case xinim::mined::Key::Right:
        return "move_right";
    case xinim::mined::Key::Home:
        return "move_line_start";
    case xinim::mined::Key::End:
        return "move_line_end";
    case xinim::mined::Key::PageUp:
        return "page_up";
    case xinim::mined::Key::PageDown:
        return "page_down";
    case xinim::mined::Key::Enter:
        return "insert_newline";
    case xinim::mined::Key::Backspace:
        return "delete_char_backward";
    case xinim::mined::Key::Delete:
        return "delete_char";
    case xinim::mined::Key::Ctrl_S:
        return "save";
    case xinim::mined::Key::Ctrl_Q:
        return "quit";
    case xinim::mined::Key::Ctrl_Z:
        return "undo";
    case xinim::mined::Key::Ctrl_Y:
        return "redo";
    case xinim::mined::Key::Character:
        return "insert_char";
    default:
        return {};
    }
}

} // namespace mined::modern::keymap
