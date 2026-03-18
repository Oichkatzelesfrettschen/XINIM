#pragma once
// Device filesystem for /dev/null, /dev/zero, /dev/tty, /dev/console.

#include "vfs.hpp"

namespace xinim::i486::devfs {

// Get the VfsOps for the devfs filesystem.
vfs::VfsOps* ops() noexcept;

} // namespace xinim::i486::devfs
