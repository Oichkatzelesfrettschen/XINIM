#ifndef XINIM_VFS_VFS_SERVER_HPP
#define XINIM_VFS_VFS_SERVER_HPP

#include <cstddef>
#include <cstdint>
#include "bare_vfs.hpp"

// Initialise VFS subsystem (inode table, dirent arena, FD table, mount table,
// root directory, standard directories). Must be called once before vfs_server_loop().
void vfs_server_init();
bool vfs_server_initialized();

// Seed a regular file into the bare-metal ramfs for early boot handoff.
// Returns 0 on success or a negative IPC_* errno value.
int vfs_seed_file(const char* path,
                  const void* data,
                  std::size_t size,
                  uint16_t mode = MODE_FILE_DEFAULT);

// Convenience wrapper for constant strings.
int vfs_seed_text(const char* path,
                  const char* text,
                  uint16_t mode = MODE_FILE_DEFAULT);

// VFS IPC message loop. Blocks on lattice_recv and dispatches VFS messages.
// Called from vfs_server_main() in bare_metal_stubs.cpp.
void vfs_server_loop();

#endif /* XINIM_VFS_VFS_SERVER_HPP */
