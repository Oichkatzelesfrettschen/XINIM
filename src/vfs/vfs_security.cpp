/**
 * @file vfs_security.cpp
 * @brief VFS Security Module Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/vfs/vfs_security.hpp>
#include <xinim/vfs/vfs.hpp>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <iostream>

namespace xinim::vfs {

VFSSecurity::VFSSecurity()
    : max_symlink_depth_(8)
    , strict_mode_(false)
    , audit_enabled_(true)
{
}

VFSSecurity& VFSSecurity::instance() {
    static VFSSecurity security;
    return security;
}

// ===== Path Validation =====

bool VFSSecurity::validate_path(const std::string& path, std::string& sanitized) {
    // Check for null bytes (potential attack)
    if (contains_null_byte(path)) {
        audit(AuditEvent::PATH_TRAVERSAL_BLOCKED, {}, path,
              "Null byte detected in path");
        return false;
    }

    // Check for empty path
    if (path.empty()) {
        return false;
    }

    // Normalize the path
    sanitized = normalize_path(path);

    // Check for path traversal attempts
    if (sanitized.find("..") != std::string::npos) {
        // After normalization, ".." should be resolved
        // If it's still present, it's likely an attack
        audit(AuditEvent::PATH_TRAVERSAL_BLOCKED, {}, path,
              "Path traversal detected after normalization");
        return false;
    }

    return true;
}

std::string VFSSecurity::normalize_path(const std::string& path) {
    if (path.empty()) {
        return "/";
    }

    std::vector<std::string> components;
    std::stringstream ss(path);
    std::string component;

    // Split path by '/'
    while (std::getline(ss, component, '/')) {
        if (component.empty() || component == ".") {
            // Skip empty components and current directory
            continue;
        }

        if (component == "..") {
            // Parent directory - pop from stack if not at root
            if (!components.empty()) {
                components.pop_back();
            }
        } else {
            // Check for dangerous components
            if (is_dangerous_component(component)) {
                std::cerr << "[VFS Security] Dangerous path component: "
                          << component << std::endl;
                // Still add it but log warning
            }
            components.push_back(component);
        }
    }

    // Reconstruct normalized path
    std::string result;
    if (components.empty()) {
        return "/";
    }

    for (const auto& comp : components) {
        result += "/" + comp;
    }

    return result;
}

bool VFSSecurity::check_path_containment(const std::string& path,
                                         const std::string& root) {
    // Normalize both paths
    std::string norm_path = normalize_path(path);
    std::string norm_root = normalize_path(root);

    // Check if path starts with root
    if (norm_path.size() < norm_root.size()) {
        return false;
    }

    if (norm_root == "/") {
        return true;  // Everything is contained in root
    }

    // Check prefix match
    if (norm_path.substr(0, norm_root.size()) != norm_root) {
        audit(AuditEvent::PATH_TRAVERSAL_BLOCKED, {}, path,
              "Path escapes root: " + root);
        return false;
    }

    // If path is longer, next char must be '/' or end of string
    if (norm_path.size() > norm_root.size() &&
        norm_path[norm_root.size()] != '/') {
        return false;
    }

    return true;
}

bool VFSSecurity::is_dangerous_component(const std::string& component) {
    // Check for hidden files (may be legitimate)
    if (!component.empty() && component[0] == '.') {
        // Not necessarily dangerous, but worth noting
    }

    // Check for very long names (potential buffer overflow)
    if (component.size() > 255) {
        return true;
    }

    // Check for unusual characters
    for (char c : component) {
        if (c < 0x20 || c == 0x7F) {
            // Control characters
            return true;
        }
    }

    return false;
}

bool VFSSecurity::contains_null_byte(const std::string& path) {
    return path.find('\0') != std::string::npos;
}

// ===== Permission Checks =====

bool VFSSecurity::check_permissions(const UserCredentials& creds,
                                    const FileAttributes& attrs,
                                    AccessMode mode) {
    // Root bypasses all permission checks (unless strict mode)
    if (creds.is_root() && !strict_mode_) {
        return true;
    }

    // Check DAC_OVERRIDE capability
    if (has_capability(creds, Capability::DAC_OVERRIDE)) {
        return true;
    }

    // Check DAC_READ_SEARCH for read operations
    if (static_cast<uint8_t>(mode) == static_cast<uint8_t>(AccessMode::READ)) {
        if (has_capability(creds, Capability::DAC_READ_SEARCH)) {
            return true;
        }
    }

    // Standard Unix permission check
    uint16_t perm_mode = attrs.permissions.mode;
    bool granted = false;

    // Check owner permissions
    if (creds.euid == attrs.uid) {
        if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::READ)) {
            granted = (perm_mode & FilePermissions::OWNER_READ) != 0;
        }
        if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::WRITE)) {
            granted = granted && (perm_mode & FilePermissions::OWNER_WRITE) != 0;
        }
        if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::EXECUTE)) {
            granted = granted && (perm_mode & FilePermissions::OWNER_EXEC) != 0;
        }
        return granted;
    }

    // Check group permissions
    if (creds.egid == attrs.gid ||
        std::find(creds.groups.begin(), creds.groups.end(), attrs.gid) != creds.groups.end()) {
        if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::READ)) {
            granted = (perm_mode & FilePermissions::GROUP_READ) != 0;
        }
        if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::WRITE)) {
            granted = granted && (perm_mode & FilePermissions::GROUP_WRITE) != 0;
        }
        if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::EXECUTE)) {
            granted = granted && (perm_mode & FilePermissions::GROUP_EXEC) != 0;
        }
        return granted;
    }

    // Check other permissions
    if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::READ)) {
        granted = (perm_mode & FilePermissions::OTHER_READ) != 0;
    }
    if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::WRITE)) {
        granted = granted && (perm_mode & FilePermissions::OTHER_WRITE) != 0;
    }
    if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::EXECUTE)) {
        granted = granted && (perm_mode & FilePermissions::OTHER_EXEC) != 0;
    }

    if (!granted) {
        audit(AuditEvent::PERMISSION_DENIED, creds, "",
              "Access denied to file");
    }

    return granted;
}

bool VFSSecurity::check_owner(const UserCredentials& creds,
                              const FileAttributes& attrs) {
    // Root or file owner
    if (creds.is_root() || creds.euid == attrs.uid) {
        return true;
    }

    // FOWNER capability
    if (has_capability(creds, Capability::FOWNER)) {
        return true;
    }

    audit(AuditEvent::PERMISSION_DENIED, creds, "",
          "Not file owner");
    return false;
}

bool VFSSecurity::check_execute(const UserCredentials& creds,
                                const FileAttributes& attrs) {
    return check_permissions(creds, attrs, AccessMode::EXECUTE);
}

// ===== Capability Checks =====

bool VFSSecurity::has_capability(const UserCredentials& creds, Capability cap) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Root has all capabilities (unless strict mode)
    if (creds.is_root() && !strict_mode_) {
        return true;
    }

    // Check if user has this capability
    auto it = capabilities_.find(creds.euid);
    if (it == capabilities_.end()) {
        return false;
    }

    uint32_t caps = it->second;
    return (caps & static_cast<uint32_t>(cap)) != 0;
}

bool VFSSecurity::grant_capability(uint32_t uid, Capability cap) {
    std::lock_guard<std::mutex> lock(mutex_);

    capabilities_[uid] |= static_cast<uint32_t>(cap);
    return true;
}

bool VFSSecurity::revoke_capability(uint32_t uid, Capability cap) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = capabilities_.find(uid);
    if (it != capabilities_.end()) {
        it->second &= ~static_cast<uint32_t>(cap);
    }

    return true;
}

// ===== Mount Point Security =====

bool VFSSecurity::check_mount_flags(const std::string& path, uint32_t flags,
                                    AccessMode mode) {
    // Check RDONLY flag
    if ((flags & MountPoint::RDONLY) &&
        (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::WRITE))) {
        audit(AuditEvent::MOUNT_VIOLATION, {}, path,
              "Write attempt on read-only filesystem");
        return false;
    }

    // Check NOEXEC flag
    if ((flags & MountPoint::NOEXEC) &&
        (static_cast<uint8_t>(mode) & static_cast<uint8_t>(AccessMode::EXECUTE))) {
        audit(AuditEvent::MOUNT_VIOLATION, {}, path,
              "Execute attempt on noexec filesystem");
        return false;
    }

    return true;
}

// ===== Symlink Protection =====

bool VFSSecurity::check_symlink_depth(int current_depth) {
    if (current_depth >= max_symlink_depth_) {
        audit(AuditEvent::SYMLINK_DEPTH_EXCEEDED, {}, "",
              "Maximum symlink depth exceeded: " + std::to_string(current_depth));
        return false;
    }
    return true;
}

bool VFSSecurity::check_symlink_safety(const std::string& link_path,
                                       const UserCredentials& creds) {
    // TODO: Implement additional symlink safety checks
    // - Check if symlink points outside allowed boundaries
    // - Detect symlink loops
    // - Protect against TOCTTOU attacks

    return true;
}

// ===== Security Audit =====

void VFSSecurity::audit(AuditEvent event, const UserCredentials& creds,
                        const std::string& path, const std::string& details) {
    if (!audit_enabled_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Limit audit log size
    if (audit_log_.size() >= MAX_AUDIT_RECORDS) {
        audit_log_.erase(audit_log_.begin());
    }

    AuditRecord record;
    record.event = event;
    record.uid = creds.uid;
    record.gid = creds.gid;
    record.path = path;
    record.details = details;
    record.timestamp = 0;  // TODO: Get actual timestamp

    audit_log_.push_back(record);

    // Print to console for now
    std::cerr << "[VFS Security Audit] Event: " << static_cast<int>(event)
              << " UID: " << creds.uid
              << " Path: " << path
              << " Details: " << details << std::endl;
}

std::vector<AuditRecord> VFSSecurity::get_audit_log(size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t start = audit_log_.size() > count ? audit_log_.size() - count : 0;
    return std::vector<AuditRecord>(audit_log_.begin() + start, audit_log_.end());
}

void VFSSecurity::clear_audit_log() {
    std::lock_guard<std::mutex> lock(mutex_);
    audit_log_.clear();
}

} // namespace xinim::vfs
