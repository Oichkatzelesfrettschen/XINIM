/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#include "minix/io/standard_streams.hpp"
#include "minix/io/file_stream.hpp"

#include <unistd.h>

namespace minix::io {

static FileStream stdin_stream{0, false};
static FileStream stdout_stream{1, true};
static FileStream stderr_stream{2, true};

Stream &stdin() { return stdin_stream; }
Stream &stdout() { return stdout_stream; }
Stream &stderr() { return stderr_stream; }

} // namespace minix::io
