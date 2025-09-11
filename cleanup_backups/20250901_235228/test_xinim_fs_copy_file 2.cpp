// commands/tests/test_xinim_fs_copy_file.cpp
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
#include <algorithm> // For std::equal
#include <iterator>  // For std::istreambuf_iterator
#include <chrono>    // For unique name generation

// POSIX includes for creating test conditions and verifying
#include <sys/stat.h> // Not strictly needed for copy_file tests, but often in test utils
#include <unistd.h>   // Not strictly needed
#include <ctime>      // For srand
#include <cstring>    // For strerror


// --- Start of Helper code (adapted from previous tests) ---
namespace { // Anonymous namespace for test helpers

class TempTestEntity {
public:
    std::filesystem::path path;
    enum class EntityType { File, Directory };

    TempTestEntity(const std::filesystem::path& base_dir, const std::string& name_prefix, EntityType type = EntityType::File,
                   const std::string& content = "default_content", bool auto_create = true) {

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
                if (outfile) {
                    outfile << content;
                } else {
                     ec_setup = std::make_error_code(std::errc::io_error);
                }
            }
            if (ec_setup) {
                 std::println(std::cerr, "FATAL: Test setup Failed to auto-create temp entity '{}': {}", path.string(), ec_setup.message());
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

bool compare_file_contents(const std::filesystem::path& p1, const std::filesystem::path& p2) {
    std::error_code ec1, ec2;
    auto size1 = std::filesystem::file_size(p1, ec1);
    if (ec1) { std::println(std::cerr, "compare_file_contents: Error getting size of '{}': {}", p1.string(), ec1.message()); return false; }
    auto size2 = std::filesystem::file_size(p2, ec2);
    if (ec2) { std::println(std::cerr, "compare_file_contents: Error getting size of '{}': {}", p2.string(), ec2.message()); return false; }

    if (size1 != size2) {
        std::println(std::cerr, "compare_file_contents: Files differ in size. P1({}): {}, P2({}): {}", p1.string(), size1, p2.string(), size2);
        return false;
    }
    if (size1 == 0) return true; // Both empty, contents match.

    std::ifstream f1(p1, std::ios::binary);
    std::ifstream f2(p2, std::ios::binary);
    if (!f1.is_open()) {std::println(std::cerr, "compare_file_contents: Could not open file: '{}'", p1.string()); return false;}
    if (!f2.is_open()) {std::println(std::cerr, "compare_file_contents: Could not open file: '{}'", p2.string()); return false;}


    std::istreambuf_iterator<char> begin1(f1), end1;
    std::istreambuf_iterator<char> begin2(f2), end2;

    bool are_equal = std::equal(begin1, end1, begin2, end2);
    if (!are_equal) {
        std::println(std::cerr, "compare_file_contents: Files '{}' and '{}' differ in content.", p1.string(), p2.string());
    }
    return are_equal;
}

} // anonymous namespace
// --- End of Helper code ---

struct CopyFileTestCase {
    std::string name;
    std::string source_content_val;
    TempTestEntity::EntityType source_type_to_create_val;

    std::string dest_suffix_val;
    bool dest_should_pre_exist_val;
    TempTestEntity::EntityType dest_pre_existing_type_val;
    std::string dest_pre_existing_content_val;

    std::filesystem::copy_options copy_options_val;
    xinim::fs::mode op_mode_for_ctx_val;

    bool expect_copy_success_val;
    std::optional<std::errc> expected_ec_on_error_val;

    std::function<void(const std::filesystem::path& /*source_path*/)> setup_action_for_source = nullptr;


    void run(const std::filesystem::path& test_case_base_path, int& failures) const {
        std::print(std::cout, "Test Case: {} (Mode: {})... ", name,
                   op_mode_for_ctx_val == xinim::fs::mode::standard ? "standard" : "direct");

        TempTestEntity source_entity(test_case_base_path, name + "_source", source_type_to_create_val, source_content_val, true);
        std::filesystem::path full_dest_path = test_case_base_path / dest_name_suffix_val;

        if (setup_action_for_source) {
            setup_action_for_source(source_entity.path);
        }

        std::filesystem::remove_all(full_dest_path);
        if (dest_should_pre_exist_val) {
            if (dest_pre_existing_type_val == TempTestEntity::EntityType::Directory) {
                 std::filesystem::create_directory(full_dest_path);
            } else {
                 std::ofstream f(full_dest_path, std::ios::binary); f << dest_pre_existing_content_val;
            }
        }

        xinim::fs::operation_context ctx;
        ctx.execution_mode = op_mode_for_ctx_val;

        auto result = xinim::fs::copy_file(source_entity.path, full_dest_path, copy_options_val, ctx);
        bool actual_op_succeeded = result.has_value();
        std::error_code actual_ec = result ? std::error_code{} : result.error();

        if (actual_op_succeeded) {
            if (expect_copy_success_val) {
                bool post_check_passed = true;
                std::error_code ec_dest_exists;
                if (!std::filesystem::exists(full_dest_path, ec_dest_exists)) {
                    std::println(std::cerr, "\n  Verification FAIL: Dest path '{}' does not exist.", full_dest_path.string()); post_check_passed = false;
                } else if (source_type_to_create_val == TempTestEntity::EntityType::File && !std::filesystem::is_regular_file(full_dest_path)) {
                    std::println(std::cerr, "\n  Verification FAIL: Dest path '{}' is not a regular file.", full_dest_path.string()); post_check_passed = false;
                } else if (source_type_to_create_val == TempTestEntity::EntityType::File) {
                    std::string expected_content_in_dest = source_content_val;
                    if (dest_should_pre_exist_val &&
                        (copy_options_val & std::filesystem::copy_options::skip_existing) != std::filesystem::copy_options::none) {
                        expected_content_in_dest = dest_pre_existing_content_val;
                    }

                    std::ifstream f_dest_check(full_dest_path, std::ios::binary);
                    std::string dest_content_after_copy((std::istreambuf_iterator<char>(f_dest_check)), std::istreambuf_iterator<char>());
                    if(dest_content_after_copy != expected_content_in_dest) {
                       std::println(std::cerr, "\n  Verification FAIL: File contents of '{}' do not match expected. Expected: '{}', Got: '{}'", full_dest_path.string(), expected_content_in_dest, dest_content_after_copy); post_check_passed = false;
                    }
                }

                std::println(std::cout, post_check_passed ? "PASS" : "FAIL (Post-conditions)");
                if(!post_check_passed) failures++;
            } else {
                std::println(std::cout, "FAIL (expected error, got success)"); failures++;
            }
        } else {
            if (expect_copy_success_val) {
                std::println(std::cout, "FAIL (expected success, got error: {})", actual_ec.message()); failures++;
            } else {
                if (expected_ec_on_error_val && actual_ec == *expected_ec_on_error_val) {
                     std::println(std::cout, "PASS (got expected error: {})", actual_ec.message());
                } else if (!expected_ec_on_error_val && actual_ec) {
                     std::println(std::cout, "PASS (any error was expected and got: {})", actual_ec.message());
                } else {
                    std::println(std::cout, "FAIL (Error mismatch. Expected: {}, Got: {})",
                                 expected_ec_on_error_val ? std::make_error_code(*expected_ec_on_error_val).message() : "any error",
                                 actual_ec.message());
                    failures++;
                }
            }
        }
        std::filesystem::remove_all(full_dest_path);
    }
};


int main() {
    int failures = 0;
    srand(static_cast<unsigned int>(time(nullptr)));

    TempTestEntity test_run_base_dir("CopyFileTestRunBase", TempTestEntity::EntityType::Directory, "", true);

    std::vector<CopyFileTestCase> test_cases = {
        {"CopyNew_Std", "Hello World Std", TempTestEntity::EntityType::File, "dest_A_std.txt", false, {}, "", std::filesystem::copy_options::none, xinim::fs::mode::standard, true, {}},
        {"CopyNew_Direct", "Hello Direct", TempTestEntity::EntityType::File, "dest_B_direct.txt", false, {}, "", std::filesystem::copy_options::none, xinim::fs::mode::direct, true, {}},
        {"CopyOverwrite_Std", "New Content Overwrite", TempTestEntity::EntityType::File, "dest_C_overwrite.txt", true, TempTestEntity::EntityType::File, "Old Content C", std::filesystem::copy_options::overwrite_existing, xinim::fs::mode::standard, true, {}},
        {"CopySkipExisting_Std", "New Content Skip", TempTestEntity::EntityType::File, "dest_D_skip.txt", true, TempTestEntity::EntityType::File, "Preserved Content D", std::filesystem::copy_options::skip_existing, xinim::fs::mode::standard, true, {}},
        {"CopyFailIfExists_Std", "Source E", TempTestEntity::EntityType::File, "dest_E_fail.txt", true, TempTestEntity::EntityType::File, "Existing Content E", std::filesystem::copy_options::none, xinim::fs::mode::standard, false, std::errc::file_exists},
        {"CopySourceDir_Std_Fails", "", TempTestEntity::EntityType::Directory, "dest_F_src_dir.txt", false, {}, "", std::filesystem::copy_options::none, xinim::fs::mode::standard, false, std::errc::is_a_directory},
        {"CopyDestParentNonExist_Std_Fails", "Source H", TempTestEntity::EntityType::File, "no_such_parent/dest_H.txt", false, {},"", std::filesystem::copy_options::none, xinim::fs::mode::standard, false, std::errc::no_such_file_or_directory},
        {"CopyNonExistentSource_Std_Fails", "Source NE", TempTestEntity::EntityType::File, "dest_G_nonexist.txt", false, {},"", std::filesystem::copy_options::none, xinim::fs::mode::standard, false, std::errc::no_such_file_or_directory,
            [](const auto& src_p){ if(std::filesystem::exists(src_p)) std::filesystem::remove(src_p); }},
    };

    for (const auto& tc : test_cases) {
        tc.run(test_run_base_dir.path, failures);
    }

    if (failures > 0) {
        std::println(std::cerr, "\n{} XINIM::FS::COPY_FILE TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::COPY_FILE TESTS PASSED.");
    return EXIT_SUCCESS;
}
