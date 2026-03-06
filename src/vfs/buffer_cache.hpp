#ifndef XINIM_VFS_BUFFER_CACHE_HPP
#define XINIM_VFS_BUFFER_CACHE_HPP

#include "bare_vfs.hpp"

// Freestanding buffer cache -- no STL, no heap

void        cache_init();
CacheBlock* cache_get(uint64_t block_num, uint32_t device_id);
void        cache_mark_dirty(CacheBlock* block);
void        cache_flush(uint32_t device_id);

#endif /* XINIM_VFS_BUFFER_CACHE_HPP */
