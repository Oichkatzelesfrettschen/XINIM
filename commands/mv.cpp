/**
 * @file mv.cpp
 * @brief Universal File/Directory Move System - C++23 Modernized
 * @details Hardware-agnostic, SIMD/FPU-ready file moving with
 *          comprehensive validation, atomic operations, permission preservation,
 *          and cross-filesystem fallback handling. Integrates with xinim::fs.
 *
 * MODERNIZATION: Complete decomposition and synthesis from legacy K&R C to
 * paradigmatically pure C++23 with RAII, exception safety, type safety,
 * vectorization readiness, and hardware abstraction.
 *
 * @author Original: Adri Koppes, Modernized for C++23 XINIM
 * @version 2.1
 * @date 2024-01-16 (xinim::fs integration)
 * @copyright XINIM OS Project
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>
#include <system_error>
#include <exception>
#include <iostream> // Required for std::cerr
#include <print>    // For std::println
#include <algorithm>
#include <ranges>
#include <chrono>
#include <unistd.h>   // For setgid, setuid, getgid, getuid
#include <sys/stat.h> // For S_IF* macros if specific checks are done.
#include <cerrno>     // For errno, EXDEV, ENOTSUP

#include "xinim/filesystem.hpp" // For xinim::fs operations


namespace xinim::commands::mv {

/**
 * @brief Universal File and Directory Move System
 */
class UniversalFileMover final {
public:
    static constexpr std::size_t MAX_PATH_LENGTH{4096};

private:
    xinim::fs::filesystem_ops fs_ops_; // New member for xinim::fs operations

public:
    UniversalFileMover() {
        if (::setgid(::getgid()) == -1 || ::setuid(::getuid()) == -1) {
            throw std::system_error(errno, std::system_category(),
                                  "Failed to drop privileges");
        }
    }

    bool move_files(const std::vector<std::filesystem::path>& source_paths,
                   const std::filesystem::path& target_path) {
        if (source_paths.empty()) {
            throw std::invalid_argument("No source files specified");
        }

        if (source_paths.size() == 1) {
            return move_single_item(source_paths[0], target_path);
        } else {
            return move_multiple_items(source_paths, target_path);
        }
    }

private:
    bool move_single_item(const std::filesystem::path& source_path,
                         const std::filesystem::path& target_path) {
        try {
            validate_paths(source_path, target_path);

            // Use get_full_status to check source existence and type
            auto status_res = fs_ops_.get_full_status(source_path, true); // Follow symlinks for source by default
            if (!status_res) {
                 std::println(std::cerr, "mv: cannot stat '{}': {}", source_path.string(), status_res.error().message());
                 return false;
            }
            // No need to check exists(status_res.value().type != file_type::not_found) as get_full_status would have errored.

            perform_move_operation(source_path, target_path);
            return true;

        } catch (const std::exception& e) { // Catches filesystem_error from perform_move_operation or validate_paths
            std::println(std::cerr, "mv: Error moving '{}': {}", source_path.string(), e.what());
            return false;
        }
    }

    bool move_multiple_items(const std::vector<std::filesystem::path>& source_paths,
                           const std::filesystem::path& target_dir) {
        bool all_success = true;
        try {
            validate_target_directory(target_dir);

            for (const auto& source_path : source_paths) {
                if (!move_single_item(source_path, construct_target_path(source_path, target_dir))) {
                    all_success = false;
                }
            }
        } catch (const std::exception& e) {
            std::println(std::cerr, "mv: {}", e.what());
            return false;
        }
        return all_success;
    }

    void validate_paths(const std::filesystem::path& source_path,
                       const std::filesystem::path& target_path) const {
        if (source_path.empty() || target_path.empty()) {
            throw std::invalid_argument("Source and target paths cannot be empty");
        }
        if (source_path.native().length() > MAX_PATH_LENGTH ||
            target_path.native().length() > MAX_PATH_LENGTH) {
            throw std::invalid_argument("Path length exceeds maximum allowed");
        }
        // Self-move check is more robustly handled in perform_move_operation using std::filesystem::equivalent
    }

    void validate_target_directory(const std::filesystem::path& target_dir) const {
        // Use get_full_status to check if target_dir is a directory
        auto status_res = fs_ops_.get_full_status(target_dir, true); // Follow symlinks
        if (!status_res) {
             throw std::invalid_argument(std::format("cannot access target directory '{}': {}", target_dir.string(), status_res.error().message()));
        }
        if (status_res.value().type != std::filesystem::file_type::directory) {
            throw std::invalid_argument(std::format("Target '{}' is not a directory", target_dir.string()));
        }
    }

    std::filesystem::path construct_target_path(const std::filesystem::path& source_path,
                                              const std::filesystem::path& target_dir) const {
        return target_dir / source_path.filename();
    }

    void perform_move_operation(const std::filesystem::path& source_p,
                              std::filesystem::path target_p) {
        std::error_code ec_equiv;
        if (std::filesystem::exists(target_p) && std::filesystem::exists(source_p) && std::filesystem::equivalent(source_p, target_p, ec_equiv)) {
             if (!ec_equiv) {
                throw std::filesystem::filesystem_error(
                    std::format("'{}' and '{}' are the same file", source_p.string(), target_p.string()),
                    source_p, target_p, std::make_error_code(std::errc::file_exists));
             }
        }
        // Ignore ec_equiv if equivalent() fails for other reasons; rename will catch it.

        std::error_code ec_status;
        auto target_fs_status = std::filesystem::status(target_p, ec_status);
        if (!ec_status && std::filesystem::is_directory(target_fs_status)) {
            target_p /= source_p.filename();
            target_fs_status = std::filesystem::status(target_p, ec_status); // Re-check
        }
        // Ignore ec_status here, let operations fail if path is problematic

        if (!ec_status && std::filesystem::exists(target_fs_status) && !std::filesystem::is_directory(target_fs_status)) {
            std::error_code ec_remove;
            std::filesystem::remove(target_p, ec_remove);
            if (ec_remove) {
                throw std::filesystem::filesystem_error(
                    std::format("cannot remove existing target file '{}'", target_p.string()),
                    target_p, ec_remove);
            }
        }

        // Attempt rename using xinim::fs layer
        auto rename_result = fs_ops_.rename_hybrid(source_p, target_p); // Defaults to auto_detect mode

        if (!rename_result) { // If rename_hybrid failed
            std::error_code ec_rename = rename_result.error();
            // EXDEV is std::errc::cross_device_link
            if (ec_rename == std::errc::cross_device_link || ec_rename.value() == EXDEV || ec_rename.value() == ENOTSUP) {
                perform_copy_and_remove(source_p, target_p);
            } else {
                throw std::filesystem::filesystem_error(
                    std::format("cannot move '{}' to '{}'", source_p.string(), target_p.string()),
                    source_p, target_p, ec_rename);
            }
        }
        // If rename_result was successful, function returns.
    }

    void perform_copy_and_remove(const std::filesystem::path& source_p,
                                const std::filesystem::path& target_p) {
        std::error_code ec_copy;
        // For copy-fallback, use the standard library's copy for now.
        // Could be replaced with xinim::fs::copy_hybrid if/when available.
        std::filesystem::copy(source_p, target_p,
            std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive, ec_copy);
        if (ec_copy) {
            throw std::filesystem::filesystem_error(
                std::format("error copying '{}' to '{}' during fallback", source_p.string(), target_p.string()),
                source_p, target_p, ec_copy);
        }

        std::error_code ec_remove;
        // Use xinim::fs for removal if available, or std::filesystem::remove_all
        // For now, assuming we want to use what's available in std::filesystem for this fallback part.
        std::filesystem::remove_all(source_p, ec_remove);
        if (ec_remove) {
            throw std::filesystem::filesystem_error(
                std::format("error removing source '{}' after copy to '{}'", source_p.string(), target_p.string()),
                source_p, ec_remove);
        }
    }
};

} // namespace xinim::commands::mv

int main(int argc, char* argv[]) noexcept {
    try {
        if (argc < 3) {
            std::println(std::cerr, "Usage: mv file1 file2 or mv dir1 dir2 or mv file1 file2 ... dir");
            return EXIT_FAILURE;
        }

        xinim::commands::mv::UniversalFileMover mover;

        std::vector<std::filesystem::path> source_paths;
        source_paths.reserve(static_cast<std::size_t>(argc - 2));

        for (int i = 1; i < argc - 1; ++i) {
            source_paths.emplace_back(argv[i]);
        }

        std::filesystem::path target_path{argv[argc - 1]};

        bool success = mover.move_files(source_paths, target_path);

        return success ? EXIT_SUCCESS : EXIT_FAILURE;

    } catch (const std::invalid_argument& e) {
        std::println(std::cerr, "mv: {}", e.what());
        return EXIT_FAILURE;
    } catch (const std::filesystem::filesystem_error& e) {
        std::println(std::cerr, "mv: {}: {} (source: '{}', target: '{}')", e.what(), e.code().message(), e.path1().string(), e.path2().string());
        return EXIT_FAILURE;
    } catch (const std::system_error& e) {
        std::println(std::cerr, "mv: {}", e.what());
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::println(std::cerr, "mv: Error: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        std::println(std::cerr, "mv: Unknown error occurred");
        return EXIT_FAILURE;
    }
}
