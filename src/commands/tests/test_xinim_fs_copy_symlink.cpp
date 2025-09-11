// commands/tests/test_xinim_fs_copy_symlink.cpp
#include "xinim/filesystem.hpp"
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
#include <sys/stat.h>   // For ::stat, ::lstat (though not directly used by copy_symlink tests for verification)
#include <unistd.h>     // For ::symlink (used by create_symlink indirectly)
#include <ctime>        // For time for srand
#include <cstring>      // For strerror


// --- Start of Helper code (adapted from previous tests) ---
namespace { // Anonymous namespace for test helpers

class TempTestEntity {
public:
    std::filesystem::path path;
    enum class EntityType { File, Directory, Symlink }; // Symlink type is for the path itself if it's a link

    TempTestEntity(const std::filesystem::path& base_dir, const std::string& name_prefix,
                   EntityType type = EntityType::File,
                   const std::string& content_or_target = "default_content",
                   bool auto_create = true) {

        auto now_chrono = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now_chrono.time_since_epoch()).count();
        static int counter = 0;
        std::string unique_suffix = "_" + std::to_string(nanos) + "_" + std::to_string(counter++);
        path = base_dir / (name_prefix + unique_suffix);

        if (auto_create) {
            std::error_code ec_setup;
            if (type == EntityType::Directory) {
                std::filesystem::create_directory(path, ec_setup);
            } else if (type == EntityType::File) {
                std::ofstream outfile(path, std::ios::binary);
                if (outfile) { outfile << content_or_target; }
                else { ec_setup = std::make_error_code(std::errc::io_error); }
            } else if (type == EntityType::Symlink) { // This means 'path' itself will be a symlink
                if (content_or_target.empty()) { throw std::runtime_error("Symlink target needed for auto-create of symlink type TempTestEntity");}
                std::filesystem::create_symlink(std::filesystem::path(content_or_target), path, ec_setup);
            }
            if (ec_setup) {
                 std::println(std::cerr, "FATAL: Test setup Failed to auto-create temp entity '{}': {}", path.string(), ec_setup.message());
                 std::exit(EXIT_FAILURE);
            }
        }
    }

    ~TempTestEntity() {
        std::error_code ec_exists;
        if (std::filesystem::exists(std::filesystem::symlink_status(path, ec_exists))) {
            std::error_code ec_remove;
            std::filesystem::remove_all(path, ec_remove);
            if (ec_remove) {
                std::println(std::cerr, "Warning: Failed to remove temporary entity {}: {}", path.string(), ec_remove.message());
            }
        } else if (ec_exists && ec_exists != std::errc::no_such_file_or_directory) {
             std::println(std::cerr, "Warning: Failed to check existence of temporary entity {}: {}", path.string(), ec_exists.message());
        }
    }
    TempTestEntity(const TempTestEntity&) = delete;
    TempTestEntity& operator=(const TempTestEntity&) = delete;
};

} // anonymous namespace
// --- End of Helper code ---

struct CopySymlinkTestCase {
    std::string name;
    std::string source_symlink_target_str;
    bool create_source_symlink_target_as_file; // If true, creates an actual file for the source symlink to point to

    std::string dest_link_name_suffix;
    bool dest_path_pre_exists;
    TempTestEntity::EntityType dest_pre_existing_type; // File or Directory

    xinim::fs::operation_context ctx_params;

    bool expect_success;
    std::optional<std::errc> expected_ec_val_on_error;

    void run(const std::filesystem::path& test_case_base_path, int& failures) const {
        std::print(std::cout, "Test Case: {} (Mode: {})... ", name,
                   ctx_params.execution_mode == xinim::fs::mode::standard ? "standard" : "direct");

        std::filesystem::path actual_source_symlink_target = source_symlink_target_str;
        std::optional<TempTestEntity> source_target_entity_holder;

        if (create_source_symlink_target_as_file) {
            // Create an actual file for the source symlink to point to
            source_target_entity_holder.emplace(test_case_base_path, name + "_src_target", TempTestEntity::EntityType::File, "target_data");
            actual_source_symlink_target = source_target_entity_holder->path;
        }
        // Else, source_symlink_target_str might be a non-existent path (for dangling symlink tests)
        // or an absolute path provided by the test case.

        TempTestEntity source_symlink_entity(test_case_base_path, name + "_source_link", TempTestEntity::EntityType::Symlink, actual_source_symlink_target.string());

        std::filesystem::path full_dest_link_path = test_case_base_path / dest_link_name_suffix;
        std::filesystem::remove_all(full_dest_link_path);
        if (dest_path_pre_exists) {
            if (dest_pre_existing_type == TempTestEntity::EntityType::Directory) std::filesystem::create_directory(full_dest_link_path);
            else { std::ofstream f(full_dest_link_path); f << "pre-existing_dest_content"; }
        }

        auto result = xinim::fs::copy_symlink(source_symlink_entity.path, full_dest_link_path, ctx_params);
        bool actual_op_succeeded = result.has_value();
        std::error_code actual_ec = result ? std::error_code{} : result.error();

        if (actual_op_succeeded) {
            if (expect_success) {
                bool post_check_passed = true;
                std::error_code ec_is_symlink;
                if (!std::filesystem::is_symlink(full_dest_link_path, ec_is_symlink) || ec_is_symlink) {
                    std::println(std::cerr, "\n  Verification FAIL: Dest path '{}' is not a symlink (or error checking). EC: {}", full_dest_link_path.string(), ec_is_symlink.message()); post_check_passed = false;
                } else {
                    std::error_code ec_read;
                    auto dest_target_content = std::filesystem::read_symlink(full_dest_link_path, ec_read);
                    if (ec_read) {
                         std::println(std::cerr, "\n  Verification FAIL: Could not read dest symlink '{}': {}", full_dest_link_path.string(), ec_read.message()); post_check_passed = false;
                    } else if (dest_target_content.string() != actual_source_symlink_target.string()) { // Compare to what source link pointed to
                         std::println(std::cerr, "\n  Verification FAIL: Dest symlink target mismatch. Expected '{}', Got '{}'.", actual_source_symlink_target.string(), dest_target_content.string()); post_check_passed = false;
                    }
                }
                std::println(std::cout, post_check_passed ? "PASS" : "FAIL (Post-conditions)");
                if(!post_check_passed) failures++;
            } else {
                std::println(std::cout, "FAIL (expected error, got success)"); failures++;
            }
        } else {
            if (expect_success) {
                std::println(std::cout, "FAIL (expected success, got error: {})", actual_ec.message()); failures++;
            } else {
                if (expected_ec_val_on_error && actual_ec == *expected_ec_val_on_error) {
                     std::println(std::cout, "PASS (got expected error: {})", actual_ec.message());
                } else if (!expected_ec_val_on_error && actual_ec) {
                     std::println(std::cout, "PASS (any error was expected and got: {})", actual_ec.message());
                } else {
                    std::println(std::cout, "FAIL (Error mismatch. Expected: {}, Got: {} ({}))",
                                 expected_ec_val_on_error ? std::make_error_code(*expected_ec_val_on_error).message() : "any error",
                                 actual_ec.value(), actual_ec.message());
                    failures++;
                }
            }
        }
        std::filesystem::remove_all(full_dest_link_path);
    }
};

int main() {
    int failures = 0;
    srand(static_cast<unsigned int>(time(nullptr)));
    xinim::fs::operation_context std_ctx; std_ctx.execution_mode = xinim::fs::mode::standard;
    xinim::fs::operation_context direct_ctx; direct_ctx.execution_mode = xinim::fs::mode::direct;

    TempTestEntity test_run_base_dir("CopySymlinkTestRunBase", TempTestEntity::EntityType::Directory);

    TempTestEntity existing_target_file(test_run_base_dir.path, "existing_target_file", TempTestEntity::EntityType::File, "target_content");
    std::filesystem::path non_existent_target = test_run_base_dir.path / "i_do_not_exist.txt";


    std::vector<CopySymlinkTestCase> test_cases = {
        {"CopyToNew_TargetExists_Std", existing_target_file.path.string(), true, "dest_s1_std.lnk", false, {}, std_ctx, true, {}},
        {"CopyToNew_TargetExists_Direct", existing_target_file.path.string(), true, "dest_s1_direct.lnk", false, {}, direct_ctx, true, {}},
        {"CopyToNew_TargetDangling_Std", non_existent_target.string(), false, "dest_s2_std_dangling.lnk", false, {}, std_ctx, true, {}}, // Copying a dangling symlink is fine

        {"Copy_SourceNotSymlink_Std_Fails", existing_target_file.path.string(), true, "dest_s3_std.lnk", false, {}, std_ctx, false, std::errc::invalid_argument}, // Source (existing_target_file.path) is a file, not symlink. read_symlink should fail.
        {"Copy_SourceNonExistent_Std_Fails", "completely_non_existent_source_symlink", false, "dest_s4_std.lnk", false, {}, std_ctx, false, std::errc::no_such_file_or_directory}, // Source symlink itself doesn't exist

        {"Copy_DestExistsAsFile_Std_Fails", existing_target_file.path.string(), true, "dest_s5_std_exists.lnk", true, TempTestEntity::EntityType::File, std_ctx, false, std::errc::file_exists},
        {"Copy_DestExistsAsDir_Std_Fails", existing_target_file.path.string(), true, "dest_s6_std_exists_dir", true, TempTestEntity::EntityType::Directory, std_ctx, false, std::errc::file_exists}, // create_symlink fails if 'to' exists
    };

    for (const auto& tc : test_cases) {
        tc.run(test_run_base_dir.path, failures);
    }

    if (failures > 0) {
        std::println(std::cerr, "\n{} XINIM::FS::COPY_SYMLINK TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::COPY_SYMLINK TESTS PASSED.");
    return EXIT_SUCCESS;
}
