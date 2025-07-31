// commands/tests/test_xinim_fs_remove.cpp
#include "xinim/filesystem.hpp" // For xinim::fs free functions and operation_context
#include <filesystem>
#include <iostream>
#include <fstream>      // For std::ofstream
#include <string>
#include <vector>
#include <cstdlib>      // For EXIT_SUCCESS, EXIT_FAILURE, rand, srand
#include <system_error> // For std::error_code, std::errc
#include <print>        // For std::println (C++23)
#include <optional>     // For std::optional
#include <numeric>      // For std::iota if needed for generating files
#include <chrono>       // For unique name generation
#include <functional>   // For std::function

// POSIX includes for creating test conditions and verifying
#include <sys/stat.h>   // For S_IRUSR etc. (though not directly used for perms here)
#include <unistd.h>     // For ::chmod (not directly used here, but related)
#include <ctime>        // For time for srand
#include <cstring>      // For strerror


// --- Start of Helper code (adapted from previous tests) ---
namespace { // Anonymous namespace for test helpers

class TempTestEntity {
public:
    std::filesystem::path path;
    enum class EntityType { File, Directory, Symlink };
    EntityType type;
    std::filesystem::path symlink_target_path_internal;

    TempTestEntity(const std::string& base_name_prefix, EntityType entity_type, const std::filesystem::path& target_for_symlink = "") {
        type = entity_type;
        symlink_target_path_internal = target_for_symlink;

        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        static int counter = 0;
        std::string unique_id = base_name_prefix + "_" + std::to_string(nanos) + "_" + std::to_string(counter++);
        path = std::filesystem::temp_directory_path() / unique_id;

        std::error_code ec_setup;
        if (type == EntityType::Directory) {
            std::filesystem::create_directory(path, ec_setup);
        } else if (type == EntityType::File) {
            std::ofstream outfile(path);
            if (outfile) {
                outfile << "default_content";
            } else {
                ec_setup = std::make_error_code(std::errc::io_error);
            }
        } else if (type == EntityType::Symlink) {
            if (symlink_target_path_internal.empty()) {
                 std::println(std::cerr, "FATAL: Symlink target must be provided for TempTestEntity symlink type in test setup for: {}", base_name_prefix);
                 std::exit(EXIT_FAILURE);
            }
            std::filesystem::create_symlink(symlink_target_path_internal, path, ec_setup);
        }

        if (ec_setup) {
             std::println(std::cerr, "FATAL: Failed to create temporary entity '{}' (type {}): {}", path.string(), static_cast<int>(type), ec_setup.message());
             std::exit(EXIT_FAILURE);
        }
    }

    ~TempTestEntity() {
        std::error_code ec_rm_exists_check;
        // Use symlink_status for exists check as path could be a dangling symlink after test
        if (std::filesystem::exists(std::filesystem::symlink_status(path, ec_rm_exists_check))) {
            std::error_code ec_rm;
            std::filesystem::remove_all(path, ec_rm);
             if (ec_rm) {
                std::println(std::cerr, "Warning: Failed to remove temporary entity {}: {}", path.string(), ec_rm.message());
            }
        } else if (ec_rm_exists_check && ec_rm_exists_check != std::errc::no_such_file_or_directory) {
            // Error trying to check existence, other than "not found"
             std::println(std::cerr, "Warning: Failed to check existence of temporary entity {}: {}", path.string(), ec_rm_exists_check.message());
        }

    }

    TempTestEntity(const TempTestEntity&) = delete;
    TempTestEntity& operator=(const TempTestEntity&) = delete;

    void create_nested_structure(int depth, int files_per_dir, int dirs_per_dir) {
        if (type != EntityType::Directory || depth <= 0) return;

        std::function<void(const std::filesystem::path&, int)> create_level =
            [&](const std::filesystem::path& current_base, int current_depth) {
            if (current_depth > depth) return;

            for (int i = 0; i < files_per_dir; ++i) {
                std::ofstream f(current_base / ("file" + std::to_string(current_depth) + "_" + std::to_string(i) + ".txt"));
                if(f) f << "content";
            }

            if (current_depth < depth) {
                for (int i = 0; i < dirs_per_dir; ++i) {
                    std::filesystem::path subdir_path = current_base / ("subdir" + std::to_string(current_depth) + "_" + std::to_string(i));
                    std::filesystem::create_directory(subdir_path);
                    create_level(subdir_path, current_depth + 1);
                }
            }
        };
        create_level(path, 1);
    }

    std::uintmax_t count_items_recursive() const {
        std::error_code ec_count;
        if (!std::filesystem::exists(std::filesystem::symlink_status(path, ec_count))) return 0;
        if (!std::filesystem::is_directory(std::filesystem::symlink_status(path, ec_count))) return 1;

        std::uintmax_t count = 1;
        for(const auto& entry : std::filesystem::recursive_directory_iterator(
                path, std::filesystem::directory_options::skip_permission_denied)) {
            (void)entry;
            count++;
        }
        return count;
    }
};
} // anonymous namespace
// --- End of Helper code ---

struct RemoveTestCase {
    std::string name;
    TempTestEntity::EntityType entity_type;
    std::string entity_name_suffix;
    bool use_remove_all;
    xinim::fs::mode op_mode_for_ctx; // Renamed from op_mode to avoid conflict if ctx is member

    std::uintmax_t expected_return_val_on_success;
    bool expect_success;
    std::optional<std::errc> expected_ec_val_on_error;
    std::function<void(TempTestEntity&)> setup_action = nullptr;
    std::filesystem::path symlink_target_path_for_setup; // Used if entity_type is Symlink for setup

    void run(int& failures) const { // fs_ops removed, will call free functions
        std::print(std::cout, "Test Case: {} (Op: {}, Mode: {})... ",
                   name,
                   use_remove_all ? "remove_all" : "remove",
                   op_mode_for_ctx == xinim::fs::mode::standard ? "standard" : "direct");

        TempTestEntity entity(name + entity_name_suffix, entity_type, symlink_target_path_for_setup);

        if (setup_action) {
            setup_action(entity);
        }

        std::uintmax_t count_before_removal = 0;
        if (use_remove_all && expect_success) {
            std::error_code ec_exists_check;
            if (std::filesystem::exists(std::filesystem::symlink_status(entity.path, ec_exists_check))) {
                 count_before_removal = entity.count_items_recursive();
            } else {
                 count_before_removal = 0;
            }
        }

        xinim::fs::operation_context ctx;
        ctx.execution_mode = op_mode_for_ctx;
        // ctx.follow_symlinks not directly relevant for remove/remove_all on the path itself,
        // as they operate on the given path (symlink itself is removed).

        std::expected<std::uintmax_t, std::error_code> result_all;
        std::expected<void, std::error_code> result_single;

        if (use_remove_all) {
            result_all = xinim::fs::remove_all(entity.path, ctx);
        } else {
            result_single = xinim::fs::remove(entity.path, ctx);
        }

        bool actual_success = use_remove_all ? result_all.has_value() : result_single.has_value();
        std::error_code actual_ec = use_remove_all ? (result_all ? std::error_code{} : result_all.error())
                                                  : (result_single ? std::error_code{} : result_single.error());
        std::uintmax_t actual_remove_count = use_remove_all ? (result_all ? *result_all : 0) : (actual_success ? 1 : 0);

        std::error_code ec_after_check;
        bool entity_gone = !std::filesystem::exists(std::filesystem::symlink_status(entity.path, ec_after_check));

        if (actual_success) {
            if (expect_success) {
                if (!entity_gone) {
                    std::println(std::cout, "FAIL (entity still exists after successful remove call)");
                    failures++;
                } else {
                    if (use_remove_all) {
                        if (actual_remove_count == count_before_removal) {
                             std::println(std::cout, "PASS");
                        } else {
                             std::println(std::cout, "FAIL (remove_all count mismatch. Expected: {}, Got: {})", count_before_removal, actual_remove_count);
                             failures++;
                        }
                    } else { // remove_hybrid success
                         std::println(std::cout, "PASS");
                    }
                }
            } else {
                std::println(std::cout, "FAIL (expected error, got success. Value: {})", actual_remove_count);
                failures++;
            }
        } else { // Error case
            if (expect_success) {
                std::println(std::cout, "FAIL (expected success, got error: {})", actual_ec.message());
                failures++;
            } else {
                bool error_code_matched = false;
                if (expected_ec_val_on_error) {
                    if (actual_ec == *expected_ec_val_on_error) {
                        error_code_matched = true;
                    }
                } else {
                    error_code_matched = true;
                }

                if (error_code_matched) {
                    std::println(std::cout, "PASS (got expected error: {})", actual_ec.message());
                } else {
                    std::println(std::cout, "FAIL");
                    std::println(std::cerr, "  Expected error code: {}, Got: {} ({})",
                                 expected_ec_val_on_error ? std::make_error_code(*expected_ec_val_on_error).message() : "any error",
                                 actual_ec.value(), actual_ec.message());
                    failures++;
                }
            }
        }
    }
};


int main() {
    int failures = 0;
    srand(static_cast<unsigned int>(time(nullptr)));

    // Base temporary directory for tests that don't manage their own base.
    TempTestEntity general_test_base("RemoveTestBaseDir", TempTestEntity::EntityType::Directory);

    TempTestEntity symlink_target_file("sym_target.txt", TempTestEntity::EntityType::File);


    // --- Tests for remove (was remove_hybrid) ---
    std::println(std::cout, "--- Testing xinim::fs::remove ---");
    std::vector<RemoveTestCase> remove_tests = {
        {"RemoveFile_Std", TempTestEntity::EntityType::File, "_file_std", false, xinim::fs::mode::standard, 1, true, {}},
        {"RemoveFile_Direct", TempTestEntity::EntityType::File, "_file_direct", false, xinim::fs::mode::direct, 1, true, {}},
        {"RemoveEmptyDir_Std", TempTestEntity::EntityType::Directory, "_emptydir_std", false, xinim::fs::mode::standard, 1, true, {}},
        {"RemoveEmptyDir_Direct", TempTestEntity::EntityType::Directory, "_emptydir_direct", false, xinim::fs::mode::direct, 1, true, {}},
        {"RemoveNonEmptyDir_Std_Fails", TempTestEntity::EntityType::Directory, "_nonempty_std", false, xinim::fs::mode::standard, 0, false, std::errc::directory_not_empty,
            [](TempTestEntity& e){ std::ofstream f(e.path / "child.txt"); f << "child"; }},
        {"RemoveNonEmptyDir_Direct_Fails", TempTestEntity::EntityType::Directory, "_nonempty_direct", false, xinim::fs::mode::direct, 0, false, std::errc::directory_not_empty,
            [](TempTestEntity& e){ std::ofstream f(e.path / "child.txt"); f << "child"; }},
        {"RemoveNonExistent_Std_Fails", TempTestEntity::EntityType::File, "_nonexist_std", false, xinim::fs::mode::standard, 0, false, std::errc::no_such_file_or_directory,
            [](TempTestEntity& e){ if(std::filesystem::exists(std::filesystem::symlink_status(e.path))) std::filesystem::remove(e.path); }},
        {"RemoveNonExistent_Direct_Fails", TempTestEntity::EntityType::File, "_nonexist_direct", false, xinim::fs::mode::direct, 0, false, std::errc::no_such_file_or_directory,
            [](TempTestEntity& e){ if(std::filesystem::exists(std::filesystem::symlink_status(e.path))) std::filesystem::remove(e.path); }},
    };
    for (const auto& tc : remove_tests) { tc.run(failures); }

    std::println(std::cout, "Test Case: RemoveSymlink_NotTarget_Std (remove)... ");
    TempTestEntity target_for_symlink_remove("target_remove_std.txt", TempTestEntity::EntityType::File);
    TempTestEntity symlink_to_remove("sym_remove_std", TempTestEntity::EntityType::Symlink, target_for_symlink_remove.path);

    xinim::fs::operation_context remove_sym_ctx; // Default context (standard mode)
    auto res_sym_remove = xinim::fs::remove(symlink_to_remove.path, remove_sym_ctx);
    if (res_sym_remove && !std::filesystem::exists(std::filesystem::symlink_status(symlink_to_remove.path)) && std::filesystem::exists(target_for_symlink_remove.path)) {
        std::println(std::cout, "PASS");
    } else {
        std::println(std::cout, "FAIL ({})", res_sym_remove ? "target_deleted_or_link_exists" : res_sym_remove.error().message());
        failures++;
    }


    // --- Tests for remove_all (was remove_all_hybrid) ---
    std::println(std::cout, "\n--- Testing xinim::fs::remove_all ---");
    std::vector<RemoveTestCase> remove_all_tests = {
        {"RemoveAll_File_Std", TempTestEntity::EntityType::File, "_rall_file_std", true, xinim::fs::mode::standard, 1, true, {}},
        {"RemoveAll_File_Direct", TempTestEntity::EntityType::File, "_rall_file_direct", true, xinim::fs::mode::direct, 1, true, {}},
        {"RemoveAll_EmptyDir_Std", TempTestEntity::EntityType::Directory, "_rall_emptydir_std", true, xinim::fs::mode::standard, 1, true, {}},
        {"RemoveAll_NonEmptyDir_Std", TempTestEntity::EntityType::Directory, "_rall_nonempty_std", true, xinim::fs::mode::standard, 0 /*count verified in run*/, true, {},
            [](TempTestEntity& e){ e.create_nested_structure(2,1,1); /* base/f0, base/sd0/f1 -> 4 items */ }},
        {"RemoveAll_NonExistent_Std", TempTestEntity::EntityType::File, "_rall_nonexist_std", true, xinim::fs::mode::standard, 0, true, {}, // remove_all on non-existent is OK, returns 0.
            [](TempTestEntity& e){ if(std::filesystem::exists(std::filesystem::symlink_status(e.path))) std::filesystem::remove(e.path); }},
    };
     for (const auto& tc : remove_all_tests) { tc.run(failures); }

    std::println(std::cout, "Test Case: RemoveAll_Symlink_NotTarget_Std... ");
    TempTestEntity target_for_symlink_rall("target_rall_std.txt", TempTestEntity::EntityType::File);
    TempTestEntity symlink_rall("sym_rall_std", TempTestEntity::EntityType::Symlink, target_for_symlink_rall.path);

    xinim::fs::operation_context removeall_sym_ctx; // Default context
    auto res_sym_rall = xinim::fs::remove_all(symlink_rall.path, removeall_sym_ctx);
    if (res_sym_rall && *res_sym_rall == 1 && !std::filesystem::exists(std::filesystem::symlink_status(symlink_rall.path)) && std::filesystem::exists(target_for_symlink_rall.path)) {
        std::println(std::cout, "PASS");
    } else {
        std::println(std::cout, "FAIL ({})", res_sym_rall ? "target_deleted_or_link_exists_or_count_wrong" : res_sym_rall.error().message());
        if(res_sym_rall) std::println(std::cerr, "  Count was: {}", *res_sym_rall);
        failures++;
    }


    if (failures > 0) {
        std::println(std::cerr, "\n{} XINIM::FS::REMOVE TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::REMOVE TESTS PASSED.");
    return EXIT_SUCCESS;
}
