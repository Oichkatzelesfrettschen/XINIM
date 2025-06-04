#include "minix/io/standard_streams.hpp"
#include "minix/io/file_stream.hpp"

#include <unistd.h>

namespace minix::io {

static FileStream stdin_stream{0, false};
static FileStream stdout_stream{1, true};
static FileStream stderr_stream{2, true};

Stream& stdin() { return stdin_stream; }
Stream& stdout() { return stdout_stream; }
Stream& stderr() { return stderr_stream; }

} // namespace minix::io
