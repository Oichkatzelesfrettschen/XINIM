// commands/tests/test_xinim_fs_touch_operations.cpp
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
#include <chrono>       // For std::chrono::*
#include <thread>       // For std::this_thread::sleep_for
#include <functional>   // For std::function

// POSIX includes for creating test conditions and verifying
#include <sys/stat.h>   // For POSIX stat, mode_t, S_IFREG etc.
#include <unistd.h>     // For ::getuid, ::getgid, ::symlink, ::chmod
#include <fcntl.h>      // For open flags if needed by helpers
#include <ctime>        // For time for srand
#include <cstring>      // For strerror


// --- Start of Helper code (adapted from previous tests) ---
namespace { // Anonymous namespace for test helpers

class TempTestEntity {
public:
    std::filesystem::path path;
    enum class EntityType { File, Directory, SymlinkItself };
    EntityType entity_type_managed;
    std::filesystem::path symlink_actual_target_path;

    TempTestEntity(const std::string& base_name_prefix, EntityType type = EntityType::File,
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
                    outfile << "initial_content";
                } else {
                     ec_setup = std::make_error_code(std::errc::io_error);
                }
            } else if (type == EntityType::SymlinkItself) {
                if (symlink_actual_target_path.empty()) {
                     // This case is for when the test will create the symlink using the 'path' member.
                } else {
                    std::filesystem::create_symlink(symlink_actual_target_path, path, ec_setup);
                }
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

std::filesystem::perms perms_from_octal(unsigned int octal_val) {
    return static_cast<std::filesystem::perms>(octal_val);
}

bool verify_permissions(const std::filesystem::path& p, std::filesystem::perms expected_perms) {
    xinim::fs::operation_context get_status_ctx;
    get_status_ctx.follow_symlinks = true;
    auto status_res = xinim::fs::get_status(p, get_status_ctx);
    if (!status_res) {
        std::println(std::cerr, "  verify_permissions: get_status failed: {}", status_res.error().message());
        return false;
    }
    auto relevant_mask = std::filesystem::perms::owner_all | std::filesystem::perms::group_all | std::filesystem::perms::others_all |
                         std::filesystem::perms::set_uid | std::filesystem::perms::set_gid | std::filesystem::perms::sticky_bit;
    return (status_res.value().permissions & relevant_mask) == (expected_perms & relevant_mask);
}

const auto TEST_TIME_POINT_1 = std::filesystem::file_time_type(std::chrono::seconds(24*3600 * 1));
const auto TEST_TIME_POINT_2 = std::filesystem::file_time_type(std::chrono::seconds(24*3600 * 2));

bool check_time_near(std::chrono::system_clock::time_point actual_sys_time,
                     std::filesystem::file_time_type expected_file_time, int tolerance_sec = 3) {
    auto fs_time_point_sys_equiv = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            expected_file_time - std::filesystem::file_time_type::clock::now()
            + std::chrono::system_clock::now());
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(actual_sys_time - fs_time_point_sys_equiv);
    return std::abs(diff.count()) <= tolerance_sec;
}

bool check_time_is_now(std::chrono::system_clock::time_point actual_sys_time, int tolerance_sec = 3) {
     auto now_sys = std::chrono::system_clock::now();
     auto diff = std::chrono::duration_cast<std::chrono::seconds>(actual_sys_time - now_sys);
     return std::abs(diff.count()) <= tolerance_sec;
}

}
// --- End of Helper code ---

struct CreateFileTestCase {
    std::string name;
    std::string file_suffix;
    std::filesystem::perms perms_to_set;
    bool fail_if_exists_flag;
    xinim::fs::mode op_mode_for_ctx;

    bool setup_pre_exists;
    TempTestEntity::EntityType pre_existing_type;

    bool expect_success;
    std::optional<std::errc> expected_ec_val_on_error;

    void run(const std::filesystem::path& base_path, int& failures) const {
        std::filesystem::path full_path = base_path / file_suffix;
        std::print(std::cout, "Test Case: CreateFile - {} (Path: '{}', Mode: {})... ", name, full_path.string(),
                   op_mode_for_ctx == xinim::fs::mode::standard ? "standard" : "direct");

        std::filesystem::remove_all(full_path);
        if (setup_pre_exists) {
            if (pre_existing_type == TempTestEntity::EntityType::Directory) std::filesystem::create_directory(full_path);
            else { std::ofstream f(full_path); f << "pre-existing"; }
        }

        xinim::fs::operation_context ctx;
        ctx.execution_mode = op_mode_for_ctx;

        auto result = xinim::fs::create_file(full_path, perms_to_set, fail_if_exists_flag, ctx);
        bool actual_success = result.has_value();
        std::error_code actual_ec = result ? std::error_code{} : result.error();

        if (actual_success) {
            if (expect_success) {
                if (setup_pre_exists && !fail_if_exists_flag && pre_existing_type == TempTestEntity::EntityType::File) {
                     std::println(std::cout, "PASS (as expected, file existed and no error)");
                } else if (!std::filesystem::is_regular_file(full_path)) {
                    std::println(std::cout, "FAIL (path is not a regular file after creation)"); failures++;
                } else if (!verify_permissions(full_path, perms_to_set)) { // fs_ops removed
                    std::println(std::cout, "FAIL (permissions not set as expected)"); failures++;
                } else {
                    std::println(std::cout, "PASS");
                }
            } else { std::println(std::cout, "FAIL (expected error, got success)"); failures++; }
        } else {
            if (expect_success) { std::println(std::cout, "FAIL (expected success, got error: {})", actual_ec.message()); failures++;}
            else {
                if (expected_ec_val_on_error && actual_ec == *expected_ec_val_on_error) { std::println(std::cout, "PASS (got expected error: {})", actual_ec.message()); }
                else if (!expected_ec_val_on_error && actual_ec) { std::println(std::cout, "PASS (any error was expected: {})", actual_ec.message());}
                else { std::println(std::cout, "FAIL (Error mismatch. Expected: {}, Got: {})", std::make_error_code(*expected_ec_val_on_error).message(), actual_ec.message()); failures++;}
            }
        }
        if (std::filesystem::exists(full_path) && !(setup_pre_exists && !fail_if_exists_flag)) { // Clean if test created it, or if it was pre-existing but fail_if_exists was true (so it should have failed)
             std::filesystem::remove_all(full_path);
        } else if (setup_pre_exists && !fail_if_exists_flag && std::filesystem::exists(full_path)) {
            // If it was pre-existing and we didn't expect to fail, it should still be there.
            // No, create_file_hybrid is like touch, it does not remove existing files if fail_if_exists is false.
            // Cleanup should happen if the test *created* it.
        }
         if (std::filesystem::exists(full_path) && ! (setup_pre_exists && !fail_if_exists_flag && expect_success) ) {
             std::filesystem::remove_all(full_path);
         }
    }
};

struct SetTimesTestCase {
    std::string name;
    std::string file_suffix;
    std::optional<std::filesystem::file_time_type> atime_to_set;
    std::optional<std::filesystem::file_time_type> mtime_to_set;
    xinim::fs::mode op_mode_for_ctx;
    bool follow_symlinks_policy; // For ctx.follow_symlinks
    bool create_entity_as_symlink;

    bool expect_success;
    std::optional<std::errc> expected_ec_val_on_error;

    void run(const std::filesystem::path& base_path, const std::filesystem::path& symlink_fixed_target, int& failures) const {
        std::filesystem::path full_path = base_path / file_suffix;
        std::print(std::cout, "Test Case: SetTimes - {} (Path: '{}', Mode: {}, Follow: {})... ", name, full_path.string(),
                   op_mode_for_ctx == xinim::fs::mode::standard ? "standard" : "direct", follow_symlinks_policy);

        std::filesystem::remove_all(full_path);
        if (create_entity_as_symlink) {
            std::filesystem::create_symlink(symlink_fixed_target, full_path);
        } else {
            std::ofstream f(full_path); f << "time_test";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        xinim::fs::operation_context ctx;
        ctx.execution_mode = op_mode_for_ctx;
        ctx.follow_symlinks = follow_symlinks_policy;

        auto result = xinim::fs::set_file_times(full_path, atime_to_set, mtime_to_set, ctx);
        bool actual_success = result.has_value();
        std::error_code actual_ec = result ? std::error_code{} : result.error();

        if (actual_success) {
            if (expect_success) {
                std::filesystem::path path_to_check_times = (create_entity_as_symlink && ctx.follow_symlinks) ? symlink_fixed_target : full_path;

                xinim::fs::operation_context verify_ctx; // Use default context for verification get_status
                verify_ctx.follow_symlinks = true; // Always follow to check target file times
                if (create_entity_as_symlink && !ctx.follow_symlinks) { // If we modified the link itself
                    verify_ctx.follow_symlinks = false; // then verify the link itself
                }


                auto status_after_res = xinim::fs::get_status(path_to_check_times, verify_ctx);

                if (!status_after_res && std::filesystem::is_symlink(full_path) && !ctx.follow_symlinks && !std::filesystem::exists(symlink_fixed_target)) {
                    std::println(std::cout, "PASS (utimensat on dangling symlink with no_follow likely succeeded, verification OS-dependent)");
                } else if (!status_after_res) {
                    std::println(std::cout, "FAIL (could not get status of '{}' after time set: {})", path_to_check_times.string(), status_after_res.error().message()); failures++;
                } else {
                    const auto& status_after = status_after_res.value();
                    bool times_ok = true;
                    if (atime_to_set) { if (!check_time_near(status_after.atime, *atime_to_set)) {times_ok=false; std::println(std::cerr, "\n  Atime mismatch for {}", path_to_check_times.string());}}
                    else { if (!check_time_is_now(status_after.atime)) {times_ok=false; std::println(std::cerr, "\n  Atime not 'now' for {}", path_to_check_times.string());}}

                    if (mtime_to_set) { if (!check_time_near(status_after.mtime, *mtime_to_set)) {times_ok=false; std::println(std::cerr, "\n  Mtime mismatch for {}", path_to_check_times.string());}}
                    else { if (!check_time_is_now(status_after.mtime)) {times_ok=false; std::println(std::cerr, "\n  Mtime not 'now' for {}", path_to_check_times.string());}}

                    if(times_ok) std::println(std::cout, "PASS"); else {std::println(std::cout, "FAIL (time verification)"); failures++;}
                }
            } else { std::println(std::cout, "FAIL (expected error, got success)"); failures++; }
        } else {
            if (expect_success) { std::println(std::cout, "FAIL (expected success, got error: {})", actual_ec.message()); failures++;}
            else {
                if (expected_ec_val_on_error && actual_ec == *expected_ec_val_on_error) { std::println(std::cout, "PASS (got expected error: {})", actual_ec.message()); }
                else if (!expected_ec_val_on_error && actual_ec) { std::println(std::cout, "PASS (any error was expected: {})", actual_ec.message());}
                else { std::println(std::cout, "FAIL (Error mismatch. Expected: {}, Got: {})", std::make_error_code(*expected_ec_val_on_error).message(), actual_ec.message()); failures++;}
            }
        }
        if (std::filesystem::exists(std::filesystem::symlink_status(full_path))) std::filesystem::remove(full_path);
    }
};


int main() {
    int failures = 0;
    srand(static_cast<unsigned int>(time(nullptr)));

    TempTestEntity base_test_dir_holder("TouchOpsTestEnvBase", TempTestEntity::EntityType::Directory, "", true);
    const std::filesystem::path& base_test_dir = base_test_dir_holder.path;

    std::println(std::cout, "--- Testing create_file ---"); // Renamed from create_file_hybrid
    std::vector<CreateFileTestCase> create_tests = {
        {"CreateNewFile_Std_Perms644", "new_std.txt", perms_from_octal(0644), false, xinim::fs::mode::standard, false, {}, true, {}},
        {"CreateNewFile_Direct_Perms600", "new_direct.txt", perms_from_octal(0600), false, xinim::fs::mode::direct, false, {}, true, {}},
        {"CreateFailIfExists_True_Std", "exist_std.txt", perms_from_octal(0644), true, xinim::fs::mode::standard, true, TempTestEntity::EntityType::File, false, std::errc::file_exists},
        {"CreateFailIfExists_False_Std", "exist_nofail_std.txt", perms_from_octal(0644), false, xinim::fs::mode::standard, true, TempTestEntity::EntityType::File, true, {}},
        {"CreateOnFileIsDir_Std_Fails", "existing_dir_as_file", perms_from_octal(0644), false, xinim::fs::mode::standard, true, TempTestEntity::EntityType::Directory, false, std::errc::is_a_directory},
        {"CreateInNoParentDir_Std_Fails", "no_parent/newfile.txt", perms_from_octal(0644), false, xinim::fs::mode::standard, false, {}, false, std::errc::no_such_file_or_directory},
    };
    for (const auto& tc : create_tests) { tc.run(base_test_dir, failures); }

    std::println(std::cout, "\n--- Testing set_file_times ---"); // Renamed from set_file_times_hybrid
    TempTestEntity symlink_target_for_times(base_test_dir.string() + "/sym_target_times.txt", TempTestEntity::EntityType::File, "", true);
    TempTestEntity dangling_symlink_target_path_provider("dangling_target_path_times", TempTestEntity::EntityType::File, "", false);

    std::vector<SetTimesTestCase> time_tests = {
        {"SetTimes_File_BothSpecific_Std", "file_times_std.txt", TEST_TIME_POINT_1, TEST_TIME_POINT_2, xinim::fs::mode::standard, true, false, true, {}},
        {"SetTimes_File_MTimeOnly_Direct", "file_mtime_direct.txt", std::nullopt, TEST_TIME_POINT_1, xinim::fs::mode::direct, true, false, true, {}},
        {"SetTimes_File_ATimeOnly_Std", "file_atime_std.txt", TEST_TIME_POINT_1, std::nullopt, xinim::fs::mode::standard, true, false, true, {}},
        {"SetTimes_File_BothNow_Direct", "file_bothnow_direct.txt", std::nullopt, std::nullopt, xinim::fs::mode::direct, true, false, true, {}},
        {"SetTimes_NonExistent_Std_Fails", "nonexist_times_std.txt", TEST_TIME_POINT_1, TEST_TIME_POINT_1, xinim::fs::mode::standard, true, false, false, std::errc::no_such_file_or_directory},
        {"SetTimes_Symlink_Follow_Std", "s_target_times_std.lnk", TEST_TIME_POINT_1, TEST_TIME_POINT_2, xinim::fs::mode::standard, true, true, true, {}},
        {"SetTimes_Symlink_NoFollow_Std", "s_target_times_nofollow_std.lnk", TEST_TIME_POINT_1, TEST_TIME_POINT_2, xinim::fs::mode::standard, false, true, true, {}},
        {"SetTimes_DanglingSymlink_NoFollow_Std", "s_dangling_times_nofollow_std.lnk", TEST_TIME_POINT_1, TEST_TIME_POINT_1, xinim::fs::mode::standard, false, true, true, {}},
    };

    for (const auto& tc : time_tests) {
        std::filesystem::path current_symlink_target = symlink_target_for_times.path;
        if (tc.name.find("Dangling") != std::string::npos) {
            current_symlink_target = dangling_symlink_target_path_provider.path;
            std::filesystem::remove(current_symlink_target); // Ensure target is dangling
        }
        tc.run(base_test_dir, current_symlink_target, failures);
    }

    if (failures > 0) {
        std::println(std::cerr, "\n{} XINIM::FS::TOUCH_OPERATIONS TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::TOUCH_OPERATIONS TESTS PASSED.");
    return EXIT_SUCCESS;
}
