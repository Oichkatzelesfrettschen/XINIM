#include "xinim/io/standard_streams.hpp"
#include "xinim/io/file_stream.hpp"

#include <unistd.h>

namespace minix::io {

/// FileStream providing access to the process' standard input.
static FileStream stdin_stream{0, false};
/// FileStream providing access to the process' standard output.
static FileStream stdout_stream{1, true};
/// FileStream providing access to the process' standard error.
static FileStream stderr_stream{2, true};

/// Obtain a reference to the standard input stream.
Stream &stdin() { return stdin_stream; }
/// Obtain a reference to the standard output stream.
Stream &stdout() { return stdout_stream; }
/// Obtain a reference to the standard error stream.
Stream &stderr() { return stderr_stream; }

} // namespace minix::io
