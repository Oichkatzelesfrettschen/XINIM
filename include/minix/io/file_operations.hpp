#pragma once

#include "stream.hpp"
#include <string_view>

namespace minix::io {

enum class OpenMode : unsigned {
    read = 1 << 0,
    write = 1 << 1,
    create = 1 << 2,
    exclusive = 1 << 3,
    truncate = 1 << 4,
    append = 1 << 5
};

inline OpenMode operator|(OpenMode a, OpenMode b) {
    return static_cast<OpenMode>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

inline bool operator&(OpenMode a, OpenMode b) {
    return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0;
}

struct Permissions {
    unsigned mode{0644};
};

Result<StreamPtr> open_stream(std::string_view path, OpenMode mode, Permissions perms = {});
Result<StreamPtr> create_stream(std::string_view path, Permissions perms = {});

} // namespace minix::io
