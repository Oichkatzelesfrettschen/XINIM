#include <algorithm>
#include <cassert>
#include <string_view>

#include "../mined_editor_command_names.hpp"

int main() {
    using namespace mined::modern::command;
    const auto has_move_up =
        std::ranges::any_of(movement_names, [](const auto &n) { return n == "move_up"; });
    const auto has_insert_char =
        std::ranges::any_of(editing_names, [](const auto &n) { return n == "insert_char"; });
    const auto has_save =
        std::ranges::any_of(system_names, [](const auto &n) { return n == "save"; });
    const auto has_undo =
        std::ranges::any_of(history_names, [](const auto &n) { return n == "undo"; });

    assert(has_move_up && has_insert_char && has_save && has_undo);
    return 0;
}
