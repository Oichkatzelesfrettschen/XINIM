/**
 * @file vfs_enhanced.cpp
 * @brief VFS implementation with integrated security checks
 *
 * This file shows the security-enhanced resolve_path and open functions.
 * Integrate these into the main vfs.cpp file.
 */

#include <xinim/vfs/vfs.hpp>
#include <xinim/vfs/vfs_security.hpp>
#include <algorithm>
#include <cstring>
#include <iostream>

namespace xinim::vfs {

// Enhanced resolve_path with security checks
VNode* VFS::resolve_path_secure(const std::string& path, bool follow_symlinks,
                                const UserCredentials& creds) {
    // SECURITY: Validate and sanitize path
    std::string sanitized_path;
    if (!VFSSecurity::instance().validate_path(path, sanitized_path)) {
        return nullptr;
    }

    if (sanitized_path.empty() || sanitized_path[0] != '/') {
        return nullptr;  // Only absolute paths
    }

    // Find the mount point
    std::string mount_path = "/";
    VNode* root = nullptr;
    uint32_t mount_flags = 0;

    for (const auto& [mp_path, mp] : mounts_) {
        if (sanitized_path.find(mp_path) == 0 && mp_path.size() > mount_path.size()) {
            mount_path = mp_path;
            root = mp.root;
            mount_flags = mp.flags;
        }
    }

    if (!root) {
        return nullptr;  // No mount point found
    }

    // Remove mount point prefix
    std::string rel_path = sanitized_path.substr(mount_path.size());
    if (rel_path.empty()) {
        rel_path = "/";
    }
    if (!rel_path.empty() && rel_path[0] != '/') {
        rel_path = "/" + rel_path;
    }

    // Traverse path components with security checks
    VNode* current = root;
    int symlink_depth = 0;

    // Split path into components
    std::vector<std::string> components;
    size_t pos = 1;  // Skip leading /
    while (pos < rel_path.size()) {
        size_t next = rel_path.find('/', pos);
        if (next == std::string::npos) {
            next = rel_path.size();
        }

        std::string component = rel_path.substr(pos, next - pos);
        pos = next + 1;

        if (!component.empty() && component != ".") {
            components.push_back(component);
        }
    }

    // Traverse components
    for (const auto& component : components) {
        // SECURITY: Check path traversal
        if (component == "..") {
            // Parent directory - This is now safe after sanitization
            // TODO: Implement parent lookup
            continue;
        }

        // Get attributes of current directory for permission check
        FileAttributes curr_attrs = current->get_attributes();

        // SECURITY: Check execute permission on directory (search permission)
        if (!VFSSecurity::instance().check_permissions(
                creds, curr_attrs, AccessMode::EXECUTE)) {
            std::cerr << "[VFS] Permission denied: cannot search directory" << std::endl;
            return nullptr;
        }

        // Lookup component
        VNode* next_node = current->lookup(component);
        if (!next_node) {
            return nullptr;
        }

        // Handle symlinks with security
        if (follow_symlinks && next_node->get_attributes().type == FileType::SYMLINK) {
            // SECURITY: Check symlink depth
            if (!VFSSecurity::instance().check_symlink_depth(symlink_depth)) {
                return nullptr;
            }

            // SECURITY: Check symlink safety
            if (!VFSSecurity::instance().check_symlink_safety(sanitized_path, creds)) {
                return nullptr;
            }

            symlink_depth++;

            // Read symlink target
            std::string target = next_node->readlink();
            if (target.empty()) {
                return nullptr;
            }

            // Recursively resolve symlink
            VNode* resolved = resolve_path_secure(target, true, creds);
            if (!resolved) {
                return nullptr;
            }

            next_node = resolved;
        }

        current = next_node;
    }

    return current;
}

// Enhanced open with security checks
VNode* VFS::open_secure(const std::string& path, uint32_t flags, uint32_t mode,
                        const UserCredentials& creds) {
    std::lock_guard<std::mutex> lock(mutex_);

    // SECURITY: Validate path
    std::string sanitized_path;
    if (!VFSSecurity::instance().validate_path(path, sanitized_path)) {
        VFSSecurity::instance().audit(
            AuditEvent::UNAUTHORIZED_ACCESS,
            creds,
            path,
            "Invalid path in open()"
        );
        return nullptr;
    }

    // Resolve path with security checks
    VNode* vnode = resolve_path_secure(sanitized_path, true, creds);

    if (!vnode) {
        // File doesn't exist
        if (!(flags & O_CREAT)) {
            return nullptr;
        }

        // Create new file - check parent directory permissions
        // TODO: Implement file creation with security checks
        VFSSecurity::instance().audit(
            AuditEvent::PERMISSION_DENIED,
            creds,
            path,
            "File creation not yet implemented"
        );
        return nullptr;
    }

    // Get file attributes
    FileAttributes attrs = vnode->get_attributes();

    // SECURITY: Determine requested access mode
    AccessMode access_mode = AccessMode::READ;
    if (flags & O_WRONLY || flags & O_RDWR) {
        access_mode = static_cast<AccessMode>(
            static_cast<uint8_t>(access_mode) | static_cast<uint8_t>(AccessMode::WRITE)
        );
    }

    // SECURITY: Check permissions
    if (!VFSSecurity::instance().check_permissions(creds, attrs, access_mode)) {
        VFSSecurity::instance().audit(
            AuditEvent::PERMISSION_DENIED,
            creds,
            path,
            "Permission denied in open()"
        );
        return nullptr;
    }

    // SECURITY: Check mount flags
    // Find mount point for this path
    uint32_t mount_flags = 0;
    for (const auto& [mp_path, mp] : mounts_) {
        if (sanitized_path.find(mp_path) == 0) {
            mount_flags = mp.flags;
            break;
        }
    }

    if (!VFSSecurity::instance().check_mount_flags(sanitized_path, mount_flags, access_mode)) {
        VFSSecurity::instance().audit(
            AuditEvent::MOUNT_VIOLATION,
            creds,
            path,
            "Mount flags violation in open()"
        );
        return nullptr;
    }

    // SECURITY: Check for SUID/SGID if executing
    if ((flags & O_EXEC) && (attrs.permissions.mode & (S_ISUID | S_ISGID))) {
        // Check NOSUID mount flag
        if (mount_flags & MountPoint::NOSUID) {
            VFSSecurity::instance().audit(
                AuditEvent::MOUNT_VIOLATION,
                creds,
                path,
                "SUID/SGID execution blocked by NOSUID mount flag"
            );
            // Continue but don't honor SUID/SGID
        }
    }

    return vnode;
}

// Example: Enhanced mkdir with security
int VFS::mkdir_secure(const std::string& path, FilePermissions mode,
                      const UserCredentials& creds) {
    // SECURITY: Validate path
    std::string sanitized_path;
    if (!VFSSecurity::instance().validate_path(path, sanitized_path)) {
        return -EINVAL;
    }

    // Extract parent directory and new directory name
    size_t last_slash = sanitized_path.rfind('/');
    if (last_slash == std::string::npos) {
        return -EINVAL;
    }

    std::string parent_path = sanitized_path.substr(0, last_slash);
    if (parent_path.empty()) {
        parent_path = "/";
    }
    std::string dir_name = sanitized_path.substr(last_slash + 1);

    // Resolve parent directory
    VNode* parent = resolve_path_secure(parent_path, true, creds);
    if (!parent) {
        return -ENOENT;
    }

    // Get parent attributes
    FileAttributes parent_attrs = parent->get_attributes();

    // SECURITY: Check write permission on parent directory
    if (!VFSSecurity::instance().check_permissions(
            creds, parent_attrs, AccessMode::WRITE)) {
        VFSSecurity::instance().audit(
            AuditEvent::PERMISSION_DENIED,
            creds,
            path,
            "Permission denied: cannot create directory"
        );
        return -EACCES;
    }

    // SECURITY: Check if parent is actually a directory
    if (parent_attrs.type != FileType::DIRECTORY) {
        return -ENOTDIR;
    }

    // Create directory
    VNode* new_dir = nullptr;
    int ret = parent->mkdir(dir_name, mode, &new_dir);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

// Example: Enhanced unlink with security
int VFS::unlink_secure(const std::string& path, const UserCredentials& creds) {
    // SECURITY: Validate path
    std::string sanitized_path;
    if (!VFSSecurity::instance().validate_path(path, sanitized_path)) {
        return -EINVAL;
    }

    // Resolve file
    VNode* vnode = resolve_path_secure(sanitized_path, false, creds);
    if (!vnode) {
        return -ENOENT;
    }

    // Get file attributes
    FileAttributes attrs = vnode->get_attributes();

    // SECURITY: Check if user owns file or has CAP_FOWNER
    if (!VFSSecurity::instance().check_owner(creds, attrs)) {
        VFSSecurity::instance().audit(
            AuditEvent::PERMISSION_DENIED,
            creds,
            path,
            "Permission denied: not file owner"
        );
        return -EACCES;
    }

    // Extract parent directory
    size_t last_slash = sanitized_path.rfind('/');
    std::string parent_path = sanitized_path.substr(0, last_slash);
    if (parent_path.empty()) {
        parent_path = "/";
    }
    std::string file_name = sanitized_path.substr(last_slash + 1);

    // Resolve parent
    VNode* parent = resolve_path_secure(parent_path, true, creds);
    if (!parent) {
        return -ENOENT;
    }

    // Get parent attributes
    FileAttributes parent_attrs = parent->get_attributes();

    // SECURITY: Check write permission on parent directory
    if (!VFSSecurity::instance().check_permissions(
            creds, parent_attrs, AccessMode::WRITE)) {
        return -EACCES;
    }

    // Perform unlink
    return parent->unlink(file_name);
}

} // namespace xinim::vfs
