/**
 * @file fs_init.hpp
 * @brief Filesystem Initialization Helper
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * Provides easy registration of all built-in filesystems.
 */

#pragma once

namespace xinim::vfs {

/**
 * @brief Initialize all built-in filesystems
 *
 * Registers all compiled-in filesystem drivers with the FilesystemRegistry.
 * Call this during kernel initialization before mounting any filesystems.
 *
 * Registered filesystems:
 * - tmpfs: Temporary in-memory filesystem
 *
 * @return 0 on success, -errno on error
 */
int initialize_filesystems();

} // namespace xinim::vfs
