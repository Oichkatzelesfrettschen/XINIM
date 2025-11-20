/**
 * @file vfs_security.hpp
 * @brief VFS Security Module - Access Control and Path Validation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * Provides comprehensive security checks for the VFS layer:
 * - Path traversal prevention
 * - Permission checking (DAC)
 * - Capability verification (MAC)
 * - Security audit logging
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace xinim::vfs {

// Forward declarations
struct FileAttributes;
struct FilePermissions;
enum class FileType;

/**
 * @brief User Credentials
 */
struct UserCredentials {
    uint32_t uid;           // User ID
    uint32_t gid;           // Primary group ID
    uint32_t euid;          // Effective user ID
    uint32_t egid;          // Effective group ID
    std::vector<uint32_t> groups;  // Supplementary groups

    // Special UIDs
    static constexpr uint32_t ROOT_UID = 0;

    bool is_root() const { return euid == ROOT_UID; }
};

/**
 * @brief Security Capabilities (Linux-style)
 */
enum class Capability : uint32_t {
    NONE          = 0,
    CHOWN         = (1 << 0),   // Change file ownership
    DAC_OVERRIDE  = (1 << 1),   // Bypass DAC checks
    DAC_READ_SEARCH = (1 << 2), // Bypass DAC for reading
    FOWNER        = (1 << 3),   // Bypass permission checks on operations
    FSETID        = (1 << 4),   // Set file capabilities
    KILL          = (1 << 5),   // Kill arbitrary processes
    SETGID        = (1 << 6),   // Change GID
    SETUID        = (1 << 7),   // Change UID
    SYS_ADMIN     = (1 << 8),   // System administration
    SYS_MODULE    = (1 << 9),   // Load/unload kernel modules
    SYS_RAWIO     = (1 << 10),  // Raw I/O access
    NET_ADMIN     = (1 << 11),  // Network administration
    NET_BIND_SERVICE = (1 << 12), // Bind to privileged ports
};

inline Capability operator|(Capability a, Capability b) {
    return static_cast<Capability>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

inline Capability operator&(Capability a, Capability b) {
    return static_cast<Capability>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
}

/**
 * @brief Access Mode for Permission Checks
 */
enum class AccessMode {
    READ    = 0x01,
    WRITE   = 0x02,
    EXECUTE = 0x04,
};

inline AccessMode operator|(AccessMode a, AccessMode b) {
    return static_cast<AccessMode>(
        static_cast<uint8_t>(a) | static_cast<uint8_t>(b)
    );
}

/**
 * @brief Security Audit Event Types
 */
enum class AuditEvent {
    PATH_TRAVERSAL_BLOCKED,   // Path traversal attempt detected
    PERMISSION_DENIED,        // Permission check failed
    CAPABILITY_DENIED,        // Capability check failed
    SYMLINK_DEPTH_EXCEEDED,   // Too many symlink levels
    MOUNT_VIOLATION,          // Mount flag violation (NOEXEC, NOSUID, etc.)
    PRIVILEGE_ESCALATION,     // Attempted privilege escalation
    UNAUTHORIZED_ACCESS,      // Access to unauthorized resource
};

/**
 * @brief Security Audit Record
 */
struct AuditRecord {
    AuditEvent event;
    uint32_t uid;
    uint32_t gid;
    std::string path;
    std::string details;
    uint64_t timestamp;
};

/**
 * @brief VFS Security Manager
 */
class VFSSecurity {
public:
    static VFSSecurity& instance();

    // ===== Path Validation =====

    /**
     * @brief Validate and sanitize path
     * Prevents path traversal attacks, checks for null bytes, etc.
     *
     * @param path Input path
     * @param sanitized Output sanitized path
     * @return true if path is valid, false if malicious
     */
    bool validate_path(const std::string& path, std::string& sanitized);

    /**
     * @brief Normalize path (remove .., ., duplicate slashes)
     *
     * @param path Input path
     * @return Normalized path
     */
    std::string normalize_path(const std::string& path);

    /**
     * @brief Check if path attempts traversal outside root
     *
     * @param path Normalized path
     * @param root Root directory constraint
     * @return true if path stays within root
     */
    bool check_path_containment(const std::string& path, const std::string& root);

    // ===== Permission Checks =====

    /**
     * @brief Check if user has requested access to file
     *
     * @param creds User credentials
     * @param attrs File attributes
     * @param mode Requested access mode
     * @return true if access granted
     */
    bool check_permissions(const UserCredentials& creds,
                          const FileAttributes& attrs,
                          AccessMode mode);

    /**
     * @brief Check if user can modify file attributes
     */
    bool check_owner(const UserCredentials& creds,
                    const FileAttributes& attrs);

    /**
     * @brief Check if user can execute file
     */
    bool check_execute(const UserCredentials& creds,
                      const FileAttributes& attrs);

    // ===== Capability Checks =====

    /**
     * @brief Check if user has specific capability
     */
    bool has_capability(const UserCredentials& creds, Capability cap);

    /**
     * @brief Grant capability to user (requires SYS_ADMIN)
     */
    bool grant_capability(uint32_t uid, Capability cap);

    /**
     * @brief Revoke capability from user
     */
    bool revoke_capability(uint32_t uid, Capability cap);

    // ===== Mount Point Security =====

    /**
     * @brief Check if operation is allowed on mount point
     * Enforces NOEXEC, NOSUID, NODEV, RDONLY flags
     */
    bool check_mount_flags(const std::string& path, uint32_t flags,
                          AccessMode mode);

    // ===== Symlink Protection =====

    /**
     * @brief Check symlink traversal depth
     */
    bool check_symlink_depth(int current_depth);

    /**
     * @brief Protect against symlink TOCTTOU attacks
     */
    bool check_symlink_safety(const std::string& link_path,
                             const UserCredentials& creds);

    // ===== Security Audit =====

    /**
     * @brief Log security event
     */
    void audit(AuditEvent event, const UserCredentials& creds,
              const std::string& path, const std::string& details = "");

    /**
     * @brief Get recent audit records
     */
    std::vector<AuditRecord> get_audit_log(size_t count = 100);

    /**
     * @brief Clear audit log
     */
    void clear_audit_log();

    // ===== Configuration =====

    /**
     * @brief Set maximum symlink depth
     */
    void set_max_symlink_depth(int depth) { max_symlink_depth_ = depth; }

    /**
     * @brief Enable/disable strict mode (deny by default)
     */
    void set_strict_mode(bool enabled) { strict_mode_ = enabled; }

    /**
     * @brief Enable/disable audit logging
     */
    void set_audit_enabled(bool enabled) { audit_enabled_ = enabled; }

    /**
     * @brief Check if audit logging is enabled
     */
    bool is_audit_enabled() const { return audit_enabled_; }

private:
    VFSSecurity();
    ~VFSSecurity() = default;

    // No copy
    VFSSecurity(const VFSSecurity&) = delete;
    VFSSecurity& operator=(const VFSSecurity&) = delete;

    // Helper functions
    bool is_dangerous_component(const std::string& component);
    bool contains_null_byte(const std::string& path);

    // Capability tracking (per-UID)
    std::unordered_map<uint32_t, uint32_t> capabilities_;

    // Audit log
    std::vector<AuditRecord> audit_log_;
    static constexpr size_t MAX_AUDIT_RECORDS = 10000;

    // Configuration
    int max_symlink_depth_ = 8;
    bool strict_mode_ = false;
    bool audit_enabled_ = true;

    // Mutex for thread safety
    mutable std::mutex mutex_;
};

/**
 * @brief RAII Security Context
 * Automatically checks permissions and audits on destruction
 */
class SecurityContext {
public:
    SecurityContext(const UserCredentials& creds, const std::string& operation)
        : creds_(creds), operation_(operation), success_(false) {}

    ~SecurityContext() {
        if (!success_ && VFSSecurity::instance().is_audit_enabled()) {
            VFSSecurity::instance().audit(
                AuditEvent::UNAUTHORIZED_ACCESS,
                creds_,
                "",
                "Operation: " + operation_
            );
        }
    }

    void set_success(bool success) { success_ = success; }

private:
    const UserCredentials& creds_;
    std::string operation_;
    bool success_;
};

} // namespace xinim::vfs
