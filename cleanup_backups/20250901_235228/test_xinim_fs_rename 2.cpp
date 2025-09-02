// commands/tests/test_xinim_fs_rename.cpp
#include "xinim/filesystem.hpp" // For xinim::fs free functions and operation_context
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <system_error>
#include <print>
#include <optional>
#include <chrono>       // For unique name generation
#include <functional>   // For std::function

// POSIX includes for creating test conditions and verifying
#include <sys/stat.h> // For ::stat, ::lstat
#include <unistd.h>   // For ::symlink (POSIX)
#include <fcntl.h>    // For open flags if needed by helpers
#include <ctime>      // For time for srand
#include <cstring>    // For strerror


// --- Start of Helper code (adapted from previous tests) ---
namespace { // Anonymous namespace for test helpers

class TempTestEntity {
public:
    std::filesystem::path path;
    enum class EntityType { File, Directory, Symlink };
    EntityType entity_type_managed;
    std::filesystem::path symlink_actual_target_path;

    TempTestEntity(const std::string& base_name_prefix, EntityType type,
                   const std::filesystem::path& target_for_symlink = "", bool auto_create = true) {
        entity_type_managed = type;
        symlink_actual_target_path = target_for_symlink;

        auto now_chrono = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now_chrono.time_since_epoch()).count();
        static int counter = 0;
        std::string unique_suffix = "_" + std::to_string(nanos) + "_" + std::to_string(counter++);
        path = std::filesystem::temp_directory_path() / (base_name_prefix + unique_suffix);

        if (auto_create) {
            std::error_code ec_setup;
            if (type == EntityType::Directory) {
                std::filesystem::create_directory(path, ec_setup);
            } else if (type == EntityType::File) {
                std::ofstream outfile(path);
                if (outfile) {
                    outfile << "test_content_" << base_name_prefix;
                } else {
                     ec_setup = std::make_error_code(std::errc::io_error);
                }
            } else if (type == EntityType::Symlink) {
                if (symlink_actual_target_path.empty()) {
                     std::println(std::cerr, "FATAL: Symlink target must be provided for auto-creating symlink TempTestEntity.");
                     std::exit(EXIT_FAILURE);
                }
                std::filesystem::create_symlink(symlink_actual_target_path, path, ec_setup);
            }
            if (ec_setup) {
                 std::println(std::cerr, "FATAL: Failed to auto-create temporary entity '{}' (type {}): {}", path.string(), static_cast<int>(type), ec_setup.message());
                 std::exit(EXIT_FAILURE);
            }
        }
    }

    ~TempTestEntity() {
        std::error_code ec_rm_exists_check;
        if (std::filesystem::exists(std::filesystem::symlink_status(path, ec_rm_exists_check))) {
            std::error_code ec_rm;
            std::filesystem::remove_all(path, ec_rm);
            if (ec_rm) {
                std::println(std::cerr, "Warning: Failed to remove temporary entity {}: {}", path.string(), ec_rm.message());
            }
        } else if (ec_rm_exists_check && ec_rm_exists_check != std::errc::no_such_file_or_directory) {
             std::println(std::cerr, "Warning: Failed to check existence of temporary entity {}: {}", path.string(), ec_rm_exists_check.message());
        }
    }
    TempTestEntity(const TempTestEntity&) = delete;
    TempTestEntity& operator=(const TempTestEntity&) = delete;
};

std::optional<ino_t> get_inode(const std::filesystem::path& p) {
    struct stat statbuf;
    if (::stat(p.c_str(), &statbuf) != 0) {
        // std::println(std::cerr, "Debug: get_inode for '{}' failed: {}", p.string(), strerror(errno));
        return std::nullopt;
    }
    return statbuf.st_ino;
}
} // anonymous namespace
// --- End of Helper code ---

struct RenameTestCase {
    std::string name;
    TempTestEntity::EntityType source_type;
    std::string source_name_suffix;
    std::string dest_name_suffix;

    bool dest_should_pre_exist;
    TempTestEntity::EntityType dest_pre_existing_type;
    bool dest_pre_existing_is_non_empty_dir;

    xinim::fs::mode op_mode_for_ctx; // Renamed from op_mode
    bool expect_rename_success;
    std::optional<std::errc> expected_ec_on_error;
    std::filesystem::path symlink_target_for_source; // Only if source_type is Symlink

    // setup_action is simplified: it's now mainly for creating the source entity,
    // as dest pre-existence is handled by flags.
    std::function<void(const std::filesystem::path& /*source_path*/, const std::filesystem::path& /*dest_path*/)> setup_action = nullptr;


    void run(const std::filesystem::path& test_case_base_path, int& failures) const { // fs_ops removed
        xinim::fs::operation_context ctx;
        ctx.execution_mode = op_mode_for_ctx;
        // ctx.follow_symlinks is not directly used by rename, behavior is fixed by OS.

        std::print(std::cout, "Test Case: {} (Mode: {})... ", name,
                   ctx.execution_mode == xinim::fs::mode::standard ? "standard" : "direct");

        std::filesystem::path full_source_path = test_case_base_path / source_name_suffix;
        std::filesystem::path full_dest_path = test_case_base_path / dest_name_suffix;

        // Ensure clean state for source and dest paths
        std::filesystem::remove_all(full_source_path);
        std::filesystem::remove_all(full_dest_path);

        // Create source entity based on test case
        if (name.find("NonExistentSource") == std::string::npos) { // Don't create if it's a non-existent source test
            if (source_type == TempTestEntity::EntityType::Directory) std::filesystem::create_directory(full_source_path);
            else if (source_type == TempTestEntity::EntityType::File) { std::ofstream f(full_source_path); f << "source_content"; }
            else if (source_type == TempTestEntity::EntityType::Symlink) {
                 if (symlink_target_for_source.empty()) {
                     std::println(std::cerr, "FATAL: Test case '{}' source symlink needs a target.", name);
                     failures++; return;
                 }
                 std::filesystem::create_symlink(symlink_target_for_source, full_source_path);
            }
        }

        // Setup destination if it should pre-exist
        if (dest_should_pre_exist) {
            if (dest_pre_existing_type == TempTestEntity::EntityType::Directory) {
                std::filesystem::create_directory(full_dest_path);
                if (dest_pre_existing_is_non_empty_dir) {
                    std::ofstream f(full_dest_path / "dummy.txt"); f << "dummy";
                }
            } else if (dest_pre_existing_type == TempTestEntity::EntityType::File) {
                std::ofstream f(full_dest_path); f << "pre-existing_dest_content";
            }
            // Symlink pre-existing destination not typically tested for rename target, usually file/dir
        }

        if (setup_action) { // Custom setup if needed (e.g. for non-existent source if not handled above)
            setup_action(full_source_path, full_dest_path);
        }


        auto result = xinim::fs::rename(full_source_path, full_dest_path, ctx);
        bool actual_op_succeeded = result.has_value();
        std::error_code actual_ec = result ? std::error_code{} : result.error();

        bool post_check_passed = true;
        if (actual_op_succeeded) {
            if (expect_rename_success) {
                std::error_code ec_exists_check;
                if (std::filesystem::exists(std::filesystem::symlink_status(full_source_path, ec_exists_check))) {
                    std::println(std::cerr, "\n  Verification FAIL: Source path '{}' still exists.", full_source_path.string()); post_check_passed = false;
                }
                if (!std::filesystem::exists(std::filesystem::symlink_status(full_dest_path, ec_exists_check))) {
                     std::println(std::cerr, "\n  Verification FAIL: Destination path '{}' does not exist.", full_dest_path.string()); post_check_passed = false;
                }

                if (post_check_passed) { // Further type check for destination
                     auto final_dest_status = std::filesystem::symlink_status(full_dest_path);
                     bool type_match = false;
                     if (source_type == TempTestEntity::EntityType::File && std::filesystem::is_regular_file(final_dest_status)) type_match = true;
                     else if (source_type == TempTestEntity::EntityType::Directory && std::filesystem::is_directory(final_dest_status)) type_match = true;
                     else if (source_type == TempTestEntity::EntityType::Symlink && std::filesystem::is_symlink(final_dest_status)) type_match = true;

                     if (!type_match) {
                        std::println(std::cerr, "\n  Verification FAIL: Destination path '{}' has incorrect type after rename.", full_dest_path.string()); post_check_passed = false;
                     }
                     if (source_type == TempTestEntity::EntityType::Symlink && type_match) {
                         std::error_code ec_read_link;
                         auto link_target = std::filesystem::read_symlink(full_dest_path, ec_read_link);
                         if (ec_read_link || link_target != symlink_target_for_source) {
                            std::println(std::cerr, "\n  Verification FAIL: Renamed symlink points to '{}', expected '{}'. (Read err: {})",
                                         link_target.string(), symlink_target_for_source.string(), ec_read_link.message());
                            post_check_passed = false;
                         }
                     }
                }
                 std::println(std::cout, post_check_passed ? "PASS" : "FAIL (Post-conditions)");
                 if(!post_check_passed) failures++;

            } else {
                std::println(std::cout, "FAIL (expected error, got success)"); failures++;
            }
        } else { // Operation returned an error
            if (expect_rename_success) {
                std::println(std::cout, "FAIL (expected success, got error: {})", actual_ec.message()); failures++;
            } else {
                bool error_code_matched = false;
                if (expected_ec_on_error) {
                    if (actual_ec == *expected_ec_on_error) {
                        error_code_matched = true;
                    }
                } else {
                    error_code_matched = true;
                }
                if (error_code_matched) {
                    std::println(std::cout, "PASS (got expected error: {})", actual_ec.message());
                } else {
                    std::println(std::cout, "FAIL (Error mismatch. Expected: {}, Got: {} ({}))",
                                 expected_ec_on_error ? std::make_error_code(*expected_ec_on_error).message() : "any error",
                                 actual_ec.value(), actual_ec.message());
                    failures++;
                }
            }
        }
        // Cleanup destination path if it exists from the operation
        std::filesystem::remove_all(full_dest_path);
        // Source path should be gone if rename succeeded, but try remove_all just in case of failed test/partial state
        std::filesystem::remove_all(full_source_path);
    }
};

int main() {
    int failures = 0;
    srand(static_cast<unsigned int>(time(nullptr)));

    TempTestEntity test_run_base_dir("RenameTestRunBase", TempTestEntity::EntityType::Directory, "", true);

    TempTestEntity fixed_symlink_target("fixed_sym_target_for_rename.txt", TempTestEntity::EntityType::File, "", true);

    std::vector<RenameTestCase> test_cases = {
        {"RenameFile_Std", TempTestEntity::EntityType::File, "src_f_std.txt", "dst_f_std.txt", false, {}, false, xinim::fs::mode::standard, true, {}, {}},
        {"RenameFile_Direct", TempTestEntity::EntityType::File, "src_f_dir.txt", "dst_f_dir.txt", false, {}, false, xinim::fs::mode::direct, true, {}, {}},
        {"RenameDir_Std", TempTestEntity::EntityType::Directory, "src_d_std", "dst_d_std", false, {}, false, xinim::fs::mode::standard, true, {}, {}},
        {"RenameDir_Direct", TempTestEntity::EntityType::Directory, "src_d_dir", "dst_d_dir", false, {}, false, xinim::fs::mode::direct, true, {}, {}},

        {"MoveFileToDestInDir_Std", TempTestEntity::EntityType::File, "file_to_move.txt", "existing_dir_for_move/file_to_move.txt", true, TempTestEntity::EntityType::Directory, false, xinim::fs::mode::standard, true, {}, {}},
        {"RenameOntoExistingFile_Std", TempTestEntity::EntityType::File, "src_overwrite.txt", "dst_exists.txt", true, TempTestEntity::EntityType::File, false, xinim::fs::mode::standard, true, {}, {}},

        {"RenameFileToNonEmptyDir_Std_Fails", TempTestEntity::EntityType::File, "src_file_to_dir.txt", "dst_nonempty_dir", true, TempTestEntity::EntityType::Directory, true, xinim::fs::mode::standard, false, std::errc::is_a_directory},
        {"RenameDirToNonEmptyDir_Std_Fails", TempTestEntity::EntityType::Directory, "src_dir_to_dir", "dst_nonempty_dir2", true, TempTestEntity::EntityType::Directory, true, xinim::fs::mode::standard, false, std::errc::directory_not_empty},

        {"RenameFileToEmptyDir_Std", TempTestEntity::EntityType::File, "src_file_to_empty_dir.txt", "dst_empty_dir", true, TempTestEntity::EntityType::Directory, false, xinim::fs::mode::standard, true, {}, {}},
        {"RenameDirToEmptyDir_Std", TempTestEntity::EntityType::Directory, "src_dir_to_empty_dir", "dst_empty_dir2", true, TempTestEntity::EntityType::Directory, false, xinim::fs::mode::standard, true, {}, {}},

        {"RenameNonExistentSource_Std_Fails", TempTestEntity::EntityType::File, "non_existent_source.txt", "dst_for_nonexist.txt", false, {}, false, xinim::fs::mode::standard, false, std::errc::no_such_file_or_directory, {}, {}},

        {"RenameSymlinkItself_Std", TempTestEntity::EntityType::Symlink, "my_symlink.lnk", "my_new_symlink.lnk", false, {}, false, xinim::fs::mode::standard, true, {}, fixed_symlink_target.path},
    };

    for (const auto& tc : test_cases) {
        tc.run(test_run_base_dir.path, failures);
    }

    if (failures > 0) {
        std::println(std::cerr, "\n{} XINIM::FS::RENAME TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::RENAME TESTS PASSED.");
    return EXIT_SUCCESS;
}
