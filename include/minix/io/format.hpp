#pragma once

#include "stream.hpp" // For minix::io::Stream, Result
#include <cstdarg>    // For va_list, va_start, va_arg, va_end
#include <string_view> // For format string
#include <cstddef>     // For size_t

// Forward declaration if Stream class is not fully defined when format.hpp is included by stream.hpp itself.
// namespace minix::io { class Stream; } // Not strictly needed if stream.hpp is complete enough

namespace minix::io {

// Main formatted print function declaration
// Returns Result<size_t> indicating bytes written or an error.
// Similar to C's vfprintf, but type-safe for the Stream object.
[[nodiscard]] Result<size_t> vprint_format(Stream& output_stream, const char* format_str, va_list args);

// Varargs wrapper for vprint_format (similar to C's fprintf)
[[nodiscard]] Result<size_t> print_format(Stream& output_stream, const char* format_str, ...);

// Convenience wrapper for printing to standard_output
[[nodiscard]] Result<size_t> print_stdout(const char* format_str, ...);

// Convenience wrapper for printing to standard_error
[[nodiscard]] Result<size_t> print_stderr(const char* format_str, ...);

} // namespace minix::io
