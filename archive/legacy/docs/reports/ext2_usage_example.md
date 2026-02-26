# ext2 Usage Example

This document demonstrates how to use ext2 (Second Extended Filesystem) in XINIM for persistent block storage.

## Overview

ext2 is a mature, well-documented filesystem that provides:
- **Persistent storage** on block devices (HDD, SSD, partitions)
- **Read-only support** (current implementation)
- **Full VFS integration** with standard POSIX operations
- **Efficient indexing** with direct, indirect, double-indirect, and triple-indirect blocks
- **Large file support** up to 2 TB (depending on block size)
- **Standard Unix features** (permissions, ownership, timestamps, hard links, symlinks)

## Prerequisites

1. **Block device** with ext2 filesystem (created with `mke2fs`)
2. **Filesystem initialization** during kernel boot
3. **Block device manager** to detect partitions

## Initialization

During kernel boot, initialize all filesystems:

```cpp
#include <xinim/vfs/fs_init.hpp>
#include <xinim/vfs/mount.hpp>
#include <xinim/block/blockdev.hpp>

// Initialize filesystem drivers
xinim::vfs::initialize_filesystems();

// Scan for block devices and partitions
auto& block_mgr = xinim::block::BlockDeviceManager::instance();
// Block devices are automatically registered by drivers (AHCI, NVMe, etc.)
```

## Detecting ext2 Partitions

The partition parser automatically detects ext2/ext3/ext4 partitions:

```cpp
auto& block_mgr = xinim::block::BlockDeviceManager::instance();

// List all detected devices
auto devices = block_mgr.get_all_devices();
for (const auto& dev_name : devices) {
    auto dev = block_mgr.get_device(dev_name);
    if (dev) {
        LOG_INFO("Device: %s (%llu blocks, %zu bytes/block)",
                 dev->get_name().c_str(),
                 dev->get_block_count(),
                 dev->get_block_size());
    }
}

// Example output:
// Device: sda (976773168 blocks, 512 bytes/block)
// Device: sda1 (1024000 blocks, 512 bytes/block) [Linux filesystem]
// Device: sda2 (975748096 blocks, 512 bytes/block) [Linux filesystem]
```

## Mounting ext2 Filesystem

Mount an ext2 filesystem from a block device or partition:

```cpp
auto& mount_table = xinim::vfs::MountTable::instance();

// Mount ext2 partition at /mnt
int ret = mount_table.mount("sda1", "/mnt", "ext2", "ro");
if (ret == 0) {
    LOG_INFO("ext2 mounted at /mnt");
} else {
    LOG_ERROR("Failed to mount ext2: %d", ret);
}

// Mount root filesystem
ret = mount_table.mount("sda2", "/", "ext2", "ro");
if (ret == 0) {
    LOG_INFO("Root filesystem mounted");
}
```

## Mount Options

Currently supported options (read-only mode):

- `ro` - Read-only (required, write support not yet implemented)
- `noexec` - Disable execution of binaries
- `nosuid` - Ignore suid/sgid bits
- `nodev` - No device files
- `noatime` - Don't update access times

Example:
```cpp
mount_table.mount("sda1", "/mnt", "ext2", "ro,noatime,noexec");
```

## Reading Files

Once mounted, use standard VFS operations:

```cpp
auto& vfs = xinim::vfs::VFS::instance();

// Open a file
VNode* file = vfs.open("/mnt/path/to/file.txt", O_RDONLY, 0);
if (!file) {
    LOG_ERROR("Failed to open file");
    return;
}

// Read file contents
char buffer[4096];
int bytes_read = vfs.read(file, buffer, sizeof(buffer), 0);
if (bytes_read > 0) {
    LOG_INFO("Read %d bytes: %.*s", bytes_read, bytes_read, buffer);
}

// Get file information
FileAttributes attrs;
if (vfs.stat("/mnt/path/to/file.txt", attrs) == 0) {
    LOG_INFO("File size: %llu bytes", attrs.size);
    LOG_INFO("Permissions: %o", attrs.permissions.mode);
    LOG_INFO("Owner: uid=%u gid=%u", attrs.uid, attrs.gid);
}

// Close file
vfs.close(file);
```

## Reading Directories

```cpp
auto& vfs = xinim::vfs::VFS::instance();

// List directory contents
auto entries = vfs.readdir("/mnt/home/user");
for (const auto& entry : entries) {
    LOG_INFO("Entry: %s", entry.c_str());
}

// Traverse directory tree
void traverse(const std::string& path) {
    auto entries = vfs.readdir(path);
    for (const auto& entry : entries) {
        if (entry == "." || entry == "..") continue;

        std::string full_path = path + "/" + entry;
        FileAttributes attrs;
        if (vfs.lstat(full_path, attrs) == 0) {
            if (attrs.type == FileType::DIRECTORY) {
                LOG_INFO("DIR:  %s/", full_path.c_str());
                traverse(full_path);
            } else {
                LOG_INFO("FILE: %s (%llu bytes)", full_path.c_str(), attrs.size);
            }
        }
    }
}

traverse("/mnt");
```

## Reading Symbolic Links

```cpp
auto& vfs = xinim::vfs::VFS::instance();

// Get link information (don't follow symlink)
FileAttributes attrs;
if (vfs.lstat("/mnt/path/to/symlink", attrs) == 0) {
    if (attrs.type == FileType::SYMLINK) {
        LOG_INFO("This is a symbolic link");

        // TODO: Add readlink() support to VFS
        // For now, use resolve_path() to follow the link
        VNode* target = vfs.resolve_path("/mnt/path/to/symlink", true);
        if (target) {
            LOG_INFO("Symlink target exists");
        }
    }
}
```

## Filesystem Information

```cpp
auto& mount_table = xinim::vfs::MountTable::instance();

// Get mount information
const auto* info = mount_table.get_mount_info("/mnt");
if (info) {
    auto* ext2fs = dynamic_cast<xinim::vfs::Ext2Filesystem*>(info->filesystem.get());
    if (ext2fs) {
        auto stats = ext2fs->get_stats();

        LOG_INFO("ext2 Filesystem Statistics:");
        LOG_INFO("  Block size: %llu bytes", stats.block_size);
        LOG_INFO("  Total blocks: %llu", stats.total_blocks);
        LOG_INFO("  Free blocks: %llu", stats.free_blocks);
        LOG_INFO("  Total size: %llu MB",
                 (stats.total_blocks * stats.block_size) / (1024*1024));
        LOG_INFO("  Free space: %llu MB",
                 (stats.free_blocks * stats.block_size) / (1024*1024));
        LOG_INFO("  Total inodes: %llu", stats.total_inodes);
        LOG_INFO("  Free inodes: %llu", stats.free_inodes);
    }
}

// View all mounts
mount_table.print_mounts();
```

Output:
```
=== Mounted Filesystems ===

Mount Point          Device          Type       Options
-----------          ------          ----       -------
/                    sda2            ext2       ro
/mnt                 sda1            ext2       ro,noatime

Total mounts: 2
```

## Unmounting

```cpp
auto& mount_table = xinim::vfs::MountTable::instance();

int ret = mount_table.unmount("/mnt");
if (ret == 0) {
    LOG_INFO("ext2 unmounted from /mnt");
}
```

## Current Limitations

⚠️ **Read-Only**: Current implementation is read-only
- ✅ Read files and directories
- ❌ Write files
- ❌ Create files/directories
- ❌ Delete files/directories
- ❌ Modify permissions/ownership
- ❌ Create hard/symbolic links

Write support will be added in a future update.

## Supported Features

Current ext2 implementation supports:

### ✅ Reading
- Regular files (up to 2 TB depending on block size)
- Directories (linked-list directory entries)
- Symbolic links
- Block devices, character devices, FIFOs, sockets (metadata only)

### ✅ Metadata
- File permissions (Unix mode bits)
- Owner UID/GID
- Timestamps (atime, mtime, ctime)
- Hard link count
- File size

### ✅ Advanced Features
- Direct blocks (0-11): First 12 blocks
- Single indirect blocks: Up to 256 KB (at 1KB block size)
- Double indirect blocks: Up to 64 MB
- Triple indirect blocks: Up to 16 GB
- Sparse files (holes)
- Multiple block group support

### ✅ Block Sizes
- 1024 bytes
- 2048 bytes
- 4096 bytes

### ✅ Revisions
- Revision 0 (original ext2)
- Revision 1 (dynamic inode sizes)

## On-Disk Structure

ext2 organizes data into block groups:

```
+------------------+
| Boot Block (1KB) |
+------------------+
| Superblock       | <- At offset 1024
+------------------+
| Group Descriptor |
| Table            |
+------------------+
| Block Bitmap     | <- Tracks free blocks
+------------------+
| Inode Bitmap     | <- Tracks free inodes
+------------------+
| Inode Table      | <- Contains all inodes
+------------------+
| Data Blocks      | <- File/directory data
+------------------+
```

### Special Inodes
- **1**: Bad blocks inode
- **2**: Root directory (/)
- **3**: ACL index inode
- **4**: ACL data inode
- **5**: Boot loader inode
- **6**: Undelete directory
- **7**: Reserved group descriptors
- **8**: Journal (ext3)
- **9**: Exclude inode
- **10**: Replica inode
- **11+**: Regular files/directories

## Performance

ext2 read performance depends on:
- **Block device speed**: HDD ~100 MB/s, SSD ~500 MB/s, NVMe ~3 GB/s
- **Block size**: Larger blocks = fewer I/O operations
- **File fragmentation**: Contiguous files are faster
- **Cache**: Future buffer cache will improve repeated reads

Typical read performance: **Limited by block device speed**

## Creating ext2 Filesystems

Use Linux tools to create ext2 filesystems for XINIM:

```bash
# Create ext2 filesystem on partition
sudo mke2fs -t ext2 /dev/sda1

# Create with specific block size
sudo mke2fs -t ext2 -b 4096 /dev/sda1

# Create with specific inode count
sudo mke2fs -t ext2 -N 1000000 /dev/sda1

# Create on disk image
dd if=/dev/zero of=disk.img bs=1M count=100
sudo mke2fs -t ext2 disk.img
```

## Common Use Cases

1. **Root filesystem** - Boot from ext2 partition
2. **Data partition** - Persistent storage for files
3. **External drives** - USB drives, external HDDs
4. **Virtual disks** - VM disk images
5. **Recovery** - Read Linux filesystems from rescue environment

## Troubleshooting

### Mount fails with "Invalid magic number"
- Partition is not ext2 formatted
- Wrong device/partition specified
- Disk corruption

### Mount fails with "No such device"
- Block device not registered
- AHCI/SATA driver not initialized
- Device name incorrect

### Files appear empty
- Incorrect block size detection
- Inode corruption
- Indirect block pointer corruption

### Directory listing is incomplete
- Directory entry corruption
- Invalid rec_len in directory entries

## Future Enhancements

Planned features for future releases:
- ✏️ Write support (create, modify, delete files)
- 📝 Journal support (ext3 compatibility)
- 🚀 Buffer cache for improved performance
- 🔍 Extent support (ext4 compatibility)
- 🔒 Extended attributes and ACLs
- 🗜️ Compression support
- 📊 Quota support
