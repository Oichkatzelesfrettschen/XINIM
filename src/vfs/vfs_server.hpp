#ifndef XINIM_VFS_VFS_SERVER_HPP
#define XINIM_VFS_VFS_SERVER_HPP

// Initialise VFS subsystem (inode table, dirent arena, FD table, mount table,
// root directory, standard directories). Must be called once before vfs_server_loop().
void vfs_server_init();

// VFS IPC message loop. Blocks on lattice_recv and dispatches VFS messages.
// Called from vfs_server_main() in bare_metal_stubs.cpp.
void vfs_server_loop();

#endif /* XINIM_VFS_VFS_SERVER_HPP */
