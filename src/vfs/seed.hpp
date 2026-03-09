#ifndef XINIM_VFS_SEED_HPP
#define XINIM_VFS_SEED_HPP

#include <cstddef>
#include <cstdint>

#include "bare_vfs.hpp"

int vfs_seed_file(const char* path,
                  const void* data,
                  std::size_t size,
                  uint16_t mode);

int vfs_seed_text(const char* path,
                  const char* text,
                  uint16_t mode);

#endif /* XINIM_VFS_SEED_HPP */
