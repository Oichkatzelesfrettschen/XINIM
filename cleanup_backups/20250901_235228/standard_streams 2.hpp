#pragma once

/**
 * @file standard_streams.hpp
 * @brief Accessors for process standard streams.
 */

#include "file_operations.hpp"

namespace minix::io {

/// Get the standard input stream.
Stream &stdin();
/// Get the standard output stream.
Stream &stdout();
/// Get the standard error stream.
Stream &stderr();

} // namespace minix::io
