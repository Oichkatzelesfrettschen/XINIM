#pragma once

#include <stdint.h>

namespace xinim::i486::ext2_reader {

struct NodeInfo {
    bool exists;
    bool is_directory;
    bool executable;
    bool is_symlink;
    uint32_t size;
    uint16_t mode; // Full POSIX mode bits from ext2 inode (type + rwxrwxrwx)
};

void probe() noexcept;
void set_timestamp_provider(uint32_t (*fn)() noexcept) noexcept;
bool register_bootfs_mount() noexcept;
bool query_runtime_path(const char* path, NodeInfo& info) noexcept;
bool query_persist_path(const char* path, NodeInfo& info) noexcept;
bool load_runtime_executable(const char* path,
                             const uint8_t** image,
                             uint32_t* size) noexcept;
bool load_persist_executable(const char* path,
                             const uint8_t** image,
                             uint32_t* size) noexcept;
bool create_runtime_file(const char* path, uint16_t mode) noexcept;
bool mkdir_runtime_directory(const char* path, uint16_t mode) noexcept;
bool rename_runtime_path(const char* old_path, const char* new_path) noexcept;
bool unlink_runtime_path(const char* path) noexcept;
bool rmdir_runtime_directory(const char* path) noexcept;
// Reads traverse all three ext2 indirect levels within the 32-bit byte-offset ABI.
// Storage mutations support direct and single-indirect inodes only; writes,
// truncation, unlink, and replacement reject larger trees before disk changes.
// Metadata-only operations (chmod, linking, and moving a regular file) remain valid.
bool truncate_runtime_file(const char* path, uint32_t size) noexcept;
bool chmod_runtime_file(const char* path, uint16_t mode) noexcept;
int read_runtime_file(const char* path,
                      uint32_t offset,
                      uint8_t* buffer,
                      uint32_t count) noexcept;
int write_runtime_file(const char* path,
                       uint32_t offset,
                       const uint8_t* buffer,
                       uint32_t count) noexcept;
int read_persist_file(const char* path,
                      uint32_t offset,
                      uint8_t* buffer,
                      uint32_t count) noexcept;
uint32_t build_runtime_directory_listing(const char* path,
                                         char* buffer,
                                         uint32_t capacity) noexcept;
uint32_t build_persist_directory_listing(const char* path,
                                         char* buffer,
                                         uint32_t capacity) noexcept;

bool create_symlink_runtime(const char* target, const char* linkpath) noexcept;
int readlink_runtime(const char* path, char* buf, uint32_t size) noexcept;
bool link_runtime_file(const char* existing, const char* new_path) noexcept;
bool query_runtime_path_no_follow(const char* path, NodeInfo& info) noexcept;

} // namespace xinim::i486::ext2_reader
