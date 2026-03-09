#pragma once

#include <stddef.h>
#include <stdint.h>

#include "xinim/boot/bootinfo.hpp"

namespace xinim::kernel::bootfs {

inline constexpr uint32_t O_RDONLY = 0x0000U;
inline constexpr uint32_t O_WRONLY = 0x0001U;
inline constexpr uint32_t O_RDWR = 0x0002U;
inline constexpr uint32_t O_APPEND = 0x0008U;
inline constexpr uint32_t O_CREAT = 0x0040U;
inline constexpr uint32_t O_EXCL = 0x0080U;
inline constexpr uint32_t O_TRUNC = 0x0200U;

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

int open(const char* path, uint32_t flags = 0U, uint32_t mode = 0U) noexcept;
int read(int fd, void* buffer, uint32_t count) noexcept;
int write(int fd, const void* buffer, uint32_t count) noexcept;
int close(int fd) noexcept;
int access(const char* path) noexcept;
[[nodiscard]] bool is_directory(const char* path) noexcept;

} // namespace xinim::kernel::bootfs

namespace xinim::i486 {
namespace bootfs = ::xinim::kernel::bootfs;
}
