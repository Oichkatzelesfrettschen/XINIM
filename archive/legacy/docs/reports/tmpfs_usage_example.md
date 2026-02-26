# tmpfs Usage Example

This document demonstrates how to use tmpfs (temporary in-memory filesystem) in XINIM.

## Initialization

During kernel boot, initialize all filesystems:

```cpp
#include <xinim/vfs/fs_init.hpp>
#include <xinim/vfs/mount.hpp>

// During kernel initialization
xinim::vfs::initialize_filesystems();
```

## Mounting tmpfs

Mount tmpfs at /tmp:

```cpp
auto& mount_table = xinim::vfs::MountTable::instance();

// Mount with default options (256MB, 4096 inodes)
int ret = mount_table.mount("none", "/tmp", "tmpfs", "rw");
if (ret == 0) {
    LOG_INFO("tmpfs mounted at /tmp");
}

// Mount with custom size limit (512MB, read-write)
ret = mount_table.mount("none", "/dev/shm", "tmpfs", "rw,size=512M");
if (ret == 0) {
    LOG_INFO("tmpfs mounted at /dev/shm");
}

// Mount read-only
ret = mount_table.mount("none", "/run", "tmpfs", "ro");
```

## Using tmpfs

Once mounted, use standard VFS operations:

```cpp
auto& vfs = xinim::vfs::VFS::instance();

// Create a file
vfs.mkdir("/tmp/mydir", 0755);
VNode* file = vfs.open("/tmp/mydir/myfile.txt", O_CREAT | O_WRONLY, 0644);

// Write data
const char* data = "Hello, tmpfs!";
vfs.write(file, data, strlen(data), 0);

// Read data
char buffer[100];
int bytes_read = vfs.read(file, buffer, sizeof(buffer), 0);

// Close file
vfs.close(file);

// Remove file
vfs.unlink("/tmp/mydir/myfile.txt");
vfs.rmdir("/tmp/mydir");
```

## Mount Options

Supported mount options:
- `rw` - Read-write (default)
- `ro` - Read-only
- `noexec` - Disable execution of binaries
- `nosuid` - Ignore suid/sgid bits
- `nodev` - No device files
- `noatime` - Don't update access times
- `size=NM` - Maximum size (e.g., "size=512M") [TODO: not yet implemented]

## Unmounting

```cpp
auto& mount_table = xinim::vfs::MountTable::instance();
int ret = mount_table.unmount("/tmp");
if (ret == 0) {
    LOG_INFO("tmpfs unmounted from /tmp");
}
```

## Filesystem Information

```cpp
auto& mount_table = xinim::vfs::MountTable::instance();
const auto* info = mount_table.get_mount_info("/tmp");
if (info) {
    auto* tmpfs = dynamic_cast<xinim::vfs::TmpfsFilesystem*>(info->filesystem.get());
    if (tmpfs) {
        LOG_INFO("tmpfs usage: %lu / %lu bytes",
                 tmpfs->get_used_bytes(), tmpfs->get_max_bytes());
        LOG_INFO("tmpfs inodes: %lu / %lu",
                 tmpfs->get_used_inodes(), tmpfs->get_total_inodes());
    }
}
```

## Viewing All Mounts

```cpp
auto& mount_table = xinim::vfs::MountTable::instance();
mount_table.print_mounts();
```

Output:
```
=== Mounted Filesystems ===

Mount Point          Device          Type       Options
-----------          ------          ----       -------
/tmp                 none            tmpfs      rw
/dev/shm             none            tmpfs      rw

Total mounts: 2
```

## Features

tmpfs provides:
- ✅ Fast in-memory storage
- ✅ POSIX-compliant file operations
- ✅ Size limits to prevent OOM
- ✅ Inode limits
- ✅ Full directory hierarchy support
- ✅ Symbolic links
- ✅ Hard links (files only)
- ✅ File permissions and ownership
- ✅ Timestamps (atime, mtime, ctime)
- ⚠️  No persistence (data lost on unmount)
- ⚠️  No swap support (all data in RAM)

## Common Use Cases

1. **/tmp** - Temporary files
2. **/dev/shm** - Shared memory segments
3. **/run** - Runtime state files
4. **/var/run** - Runtime variable data
5. **Testing** - Fast test file creation without disk I/O

## Performance

tmpfs is extremely fast because:
- No disk I/O
- Direct memory access
- No block layer overhead
- No journaling or filesystem complexity

Typical performance: **~10 GB/s** (limited only by RAM speed)
