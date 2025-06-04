#pragma once

#include "file_operations.hpp"

namespace minix::io {

Stream &stdin();
Stream &stdout();
Stream &stderr();

} // namespace minix::io
