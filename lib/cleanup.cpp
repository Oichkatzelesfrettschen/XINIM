#include "../include/minix/io/standard_streams.hpp" // Path relative to lib/cleanup.cpp

// _cleanup() is called by the C library's exit() function
// (or equivalent process termination handler) to flush all
// I/O buffers before program termination.
extern "C" void _cleanup() {
    minix::io::flush_all_open_streams();
}
