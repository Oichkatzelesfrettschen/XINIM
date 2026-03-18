#pragma once
// /proc filesystem for the i486 kernel.
// Exposes process state as virtual files per POSIX/Linux convention.

#include "vfs.hpp"

namespace xinim::i486::procfs {

// Get VfsOps for procfs. Mount at "/proc".
vfs::VfsOps* ops() noexcept;

} // namespace xinim::i486::procfs
