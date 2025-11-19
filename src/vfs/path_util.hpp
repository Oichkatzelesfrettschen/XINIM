/**
 * @file path_util.hpp
 * @brief Path resolution utilities for XINIM VFS
 *
 * Provides utilities for parsing and resolving filesystem paths.
 *
 * @ingroup vfs
 */

#ifndef XINIM_VFS_PATH_UTIL_HPP
#define XINIM_VFS_PATH_UTIL_HPP

#include <string>
#include <vector>
#include <memory>
#include "ramfs.hpp"

namespace xinim::vfs {

/**
 * @brief Split a path into components
 * @param path Path to split (e.g., "/foo/bar/baz")
 * @return Vector of path components (e.g., ["foo", "bar", "baz"])
 *
 * Note: Leading and trailing slashes are removed, empty components are skipped.
 */
inline std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> components;
    if (path.empty()) return components;

    size_t start = (path[0] == '/') ? 1 : 0;
    size_t end = path.length();

    while (start < end) {
        size_t pos = path.find('/', start);
        if (pos == std::string::npos) pos = end;

        if (pos > start) {
            std::string component = path.substr(start, pos - start);
            if (!component.empty() && component != ".") {
                if (component == "..") {
                    if (!components.empty()) {
                        components.pop_back();
                    }
                } else {
                    components.push_back(component);
                }
            }
        }

        start = pos + 1;
    }

    return components;
}

/**
 * @brief Normalize a path
 * @param path Path to normalize
 * @return Normalized path (e.g., "/foo/./bar/../baz" -> "/foo/baz")
 */
inline std::string normalize_path(const std::string& path) {
    if (path.empty() || path == "/") return "/";

    auto components = split_path(path);
    if (components.empty()) return "/";

    std::string result;
    for (const auto& comp : components) {
        result += "/" + comp;
    }

    return result.empty() ? "/" : result;
}

/**
 * @brief Resolve a path to a node
 * @param fs Filesystem instance
 * @param path Path to resolve
 * @param follow_symlinks Whether to follow symbolic links
 * @return Pointer to resolved node, or nullptr if not found
 */
inline std::shared_ptr<RamfsNode> resolve_path(const RamfsFilesystem& fs,
                                                 const std::string& path,
                                                 bool follow_symlinks = true) {
    if (path.empty()) return nullptr;
    if (path == "/") return fs.root();

    auto components = split_path(path);
    if (components.empty()) return fs.root();

    std::shared_ptr<RamfsNode> current = fs.root();

    for (const auto& component : components) {
        if (!current || !current->is_dir()) {
            return nullptr;  // Not a directory
        }

        auto dir = std::static_pointer_cast<RamfsDir>(current);
        current = dir->lookup(component);

        if (!current) {
            return nullptr;  // Component not found
        }

        // TODO: Handle symlinks if follow_symlinks is true
    }

    return current;
}

/**
 * @brief Resolve parent directory and filename from a path
 * @param fs Filesystem instance
 * @param path Path to resolve
 * @return Pair of (parent_dir, filename), or (nullptr, "") if invalid
 */
inline std::pair<std::shared_ptr<RamfsDir>, std::string>
resolve_parent(const RamfsFilesystem& fs, const std::string& path) {
    if (path.empty() || path == "/") {
        return {nullptr, ""};  // Cannot get parent of root
    }

    auto components = split_path(path);
    if (components.empty()) {
        return {nullptr, ""};
    }

    std::string filename = components.back();
    components.pop_back();

    if (components.empty()) {
        // Path is in root directory
        return {fs.root(), filename};
    }

    // Build parent path
    std::string parent_path;
    for (const auto& comp : components) {
        parent_path += "/" + comp;
    }

    auto parent_node = resolve_path(fs, parent_path);
    if (!parent_node || !parent_node->is_dir()) {
        return {nullptr, ""};
    }

    return {std::static_pointer_cast<RamfsDir>(parent_node), filename};
}

/**
 * @brief Get directory name from path
 * @param path Path
 * @return Directory part of path (e.g., "/foo/bar/baz.txt" -> "/foo/bar")
 */
inline std::string dirname(const std::string& path) {
    if (path.empty() || path == "/") return "/";

    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";

    return path.substr(0, pos);
}

/**
 * @brief Get base name from path
 * @param path Path
 * @return Base name (e.g., "/foo/bar/baz.txt" -> "baz.txt")
 */
inline std::string basename(const std::string& path) {
    if (path.empty() || path == "/") return "/";

    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return path;

    return path.substr(pos + 1);
}

/**
 * @brief Check if path is absolute
 */
inline bool is_absolute(const std::string& path) noexcept {
    return !path.empty() && path[0] == '/';
}

/**
 * @brief Join two paths
 * @param dir Directory path
 * @param file File name or relative path
 * @return Joined path
 */
inline std::string join_path(const std::string& dir, const std::string& file) {
    if (dir.empty()) return file;
    if (file.empty()) return dir;
    if (is_absolute(file)) return file;

    if (dir.back() == '/') {
        return dir + file;
    } else {
        return dir + "/" + file;
    }
}

} // namespace xinim::vfs

#endif /* XINIM_VFS_PATH_UTIL_HPP */
