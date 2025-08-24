#include <cassert>
#include <string_view>

#include "../mined_key_command_map.hpp"

int main() {
    using xinim::mined::Key;
    using namespace mined::modern;
    assert(keymap::command_for_key(Key::Up) == "move_up");
    assert(keymap::command_for_key(Key::Ctrl_S) == "save");
    assert(keymap::command_for_key(static_cast<Key>(0)).empty());
    return 0;
}
