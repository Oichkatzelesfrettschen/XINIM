#pragma once

#include "stream.hpp" // For minix::io::Result and other common types
#include <cstddef>    // For size_t, ptrdiff_t
#include <cstdint>    // For fixed-width integers if needed for syscall args
#include <fcntl.h>    // For O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_APPEND, O_TRUNC etc.
#include <sys/types.h> // For mode_t (though MINIX might have its own)

// Define common open flags if not directly using fcntl.h versions,
// or to map them to MINIX-specific values.
// For now, assume fcntl.h constants are usable or will be mapped.

namespace minix::io::syscall {

// File descriptor type, consistent with underlying system if possible
using fd_t = int;
inline constexpr fd_t invalid_fd = -1;

// Standard POSIX-like whence values for lseek
// These typically map to SEEK_SET, SEEK_CUR, SEEK_END from <unistd.h> or <stdio.h>
// but defining them here avoids direct dependency on those C headers if not desired.
enum class LseekWhence : int {
    set = 0, // SEEK_SET
    cur = 1, // SEEK_CUR
    end = 2  // SEEK_END
};

// Syscall wrapper declarations
// These functions will eventually translate to MINIX message passing.

[[nodiscard]] Result<fd_t> open(const char* path, int flags, int mode = 0666);
[[nodiscard]] Result<void> close(fd_t fd);
[[nodiscard]] Result<size_t> read(fd_t fd, void* buffer, size_t count);
[[nodiscard]] Result<size_t> write(fd_t fd, const void* buffer, size_t count);
[[nodiscard]] Result<size_t> lseek(fd_t fd, std::ptrdiff_t offset, LseekWhence whence);

// MINIX-specific message passing helper (example, might need different signature)
// This is a placeholder for what will be the core of MINIX syscalls.
// It might not return Result<void> but rather a more complex message structure or error code.
// For now, keep it simple for stubbing.
// struct MinixMessage { /* ... fields ... */ }; // Define if needed
// [[nodiscard]] Result<MinixMessage> send_receive(int process_nr, MinixMessage* msg_ptr);

} // namespace minix::io::syscall
