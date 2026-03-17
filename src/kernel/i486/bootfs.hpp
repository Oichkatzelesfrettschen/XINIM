#pragma once

#include <stddef.h>
#include <stdint.h>

#include "xinim/userland/userspace_stat.hpp"
#include "xinim/boot/bootinfo.hpp"

namespace xinim::kernel::bootfs {

inline constexpr uint32_t O_RDONLY = 0x0000U;
inline constexpr uint32_t O_WRONLY = 0x0001U;
inline constexpr uint32_t O_RDWR = 0x0002U;
inline constexpr uint32_t O_APPEND = 0x0008U;
inline constexpr uint32_t O_CREAT = 0x0040U;
inline constexpr uint32_t O_EXCL = 0x0080U;
inline constexpr uint32_t O_TRUNC = 0x0200U;
inline constexpr int kReadWouldBlock = -2;
inline constexpr int kWriteWouldBlock = -3;

using UserspaceStat = ::xinim::userland::UserspaceStat;

struct FileRecord {
    const char* path;
    uint8_t* data;
    uint32_t size;
    uint32_t capacity;
    bool read_only;
    bool executable;
    bool is_directory;
};

using VisitCallback = bool (*)(const FileRecord& file, void* context);

void initialize(const xinim::boot::BootInfo& info) noexcept;
const FileRecord* find(const char* path) noexcept;
void for_each_entry(VisitCallback callback, void* context) noexcept;
int register_directory(const char* path) noexcept;
int register_mount_directory(const char* path) noexcept;
int register_read_only_file(const char* path,
                            uint8_t* data,
                            uint32_t size,
                            bool executable = false) noexcept;

int open(const char* path, uint32_t flags = 0U, uint32_t mode = 0U) noexcept;
int read(int fd, void* buffer, uint32_t count) noexcept;
int write(int fd, const void* buffer, uint32_t count) noexcept;
int close(int fd) noexcept;
bool is_open(int fd) noexcept;
bool is_console_fd(int fd) noexcept;
int mkdir(const char* path, uint32_t mode) noexcept;
int rename(const char* old_path, const char* new_path) noexcept;
int rmdir(const char* path) noexcept;
int unlink(const char* path) noexcept;
int access(const char* path) noexcept;
int duplicate(int old_fd, int new_fd = -1) noexcept;
int make_pipe(int fd_array[2]) noexcept;
int64_t seek(int fd, int64_t offset, int whence) noexcept;
int stat_path(const char* path, UserspaceStat* buffer) noexcept;
int stat_fd(int fd, UserspaceStat* buffer) noexcept;
int control(int fd, int command, uintptr_t argument) noexcept;
int descriptor_flags(int fd) noexcept;
int set_descriptor_flags(int fd, int flags) noexcept;
int status_flags(int fd) noexcept;
int set_status_flags(int fd, int flags) noexcept;
[[nodiscard]] bool is_directory(const char* path) noexcept;
void close_cloexec_fds() noexcept;
[[nodiscard]] const char* directory_path_for_fd(int fd) noexcept;

[[nodiscard]] int foreground_pgrp() noexcept;

// Refcount management for per-process fd table support
void increment_slot_refcount(int slot) noexcept;
void decrement_slot_refcount(int slot) noexcept;
[[nodiscard]] int descriptor_flags_for_slot(int slot) noexcept;
// Increment pipe reader/writer count when duplicating a pipe fd
void increment_pipe_users(int slot) noexcept;

} // namespace xinim::kernel::bootfs

namespace xinim::i486 {
namespace bootfs = ::xinim::kernel::bootfs;
}
