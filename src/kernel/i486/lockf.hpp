#pragma once
// Advisory file/record locking per POSIX.1 fcntl(F_SETLK) and BSD flock().
// Cleanroom implementation -- simple lock table, no deadlock detection.

#include <stdint.h>

namespace xinim::i486::lockf {

// flock(2) operations
constexpr int kLockSh = 1;  // LOCK_SH
constexpr int kLockEx = 2;  // LOCK_EX
constexpr int kLockNb = 4;  // LOCK_NB
constexpr int kLockUn = 8;  // LOCK_UN

// fcntl lock types (struct flock l_type)
constexpr int16_t kFRdlck = 0;
constexpr int16_t kFWrlck = 1;
constexpr int16_t kFUnlck = 2;

// fcntl commands
constexpr int kFGetlk = 5;
constexpr int kFSetlk = 6;
constexpr int kFSetlkw = 7;

// struct flock (i386 layout, 16 bytes)
struct Flock32 {
    int16_t l_type;
    int16_t l_whence;
    int32_t l_start;
    int32_t l_len;    // 0 = to EOF
    int32_t l_pid;
};

// Acquire or release a BSD flock on a file descriptor.
int do_flock(int global_fd, int operation, uint32_t pid) noexcept;

// Process fcntl F_GETLK/F_SETLK/F_SETLKW.
int do_fcntl_lock(int global_fd, int cmd, Flock32* fl, uint32_t pid) noexcept;

// Release all locks held by a given pid on a given fd (called on close).
void release_locks_for_fd(int global_fd, uint32_t pid) noexcept;

} // namespace xinim::i486::lockf
