// commands/tests/test_xinim_fs_create_directories.cpp
#include "xinim/filesystem.hpp" // Adjust path as needed
#include <filesystem>
#include <iostream>
#include <fstream>      // For std::ofstream
#include <string>
#include <vector>
#include <cstdlib>      // For EXIT_SUCCESS, EXIT_FAILURE, rand, srand
#include <system_error> // For std::error_code, std::errc
#include <print>        // For std::println (C++23)
#include <optional>     // For std::optional
#include <chrono>       // For unique name generation

// POSIX includes for verifying permissions
#include <sys/stat.h>   // For ::stat, S_IRUSR etc.
#include <unistd.h>     // For ::stat (POSIX)
#include <ctime>        // For time for srand
#include <cstring>      // For strerror


// --- Start of Helper code (could be in a shared test_utils.hpp) ---
class TempTestDir {
public:
    std::filesystem::path path;
    TempTestDir(const std::string& base_name = "test_cdh_dir") { // cdh for create_directories_hybrid
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        static int counter = 0;
        path = std::filesystem::temp_directory_path() / (base_name + "_" + std::to_string(nanos) + "_" + std::to_string(counter++));
        // IMPORTANT: TempTestDir for create_directories_hybrid tests should NOT create the base path itself
        // if the base_name itself is the path to be created by the function under test.
        // However, if it's a *parent* for test paths, it should be created.
        // The current usage in main creates a base (e.g. test_base_dir_holder) that *is* created.
    }

    ~TempTestDir() {
        std::error_code ec;
        // Check if path was actually created by a test before attempting removal
        if (std::filesystem::exists(path)) {
            std::filesystem::remove_all(path, ec);
            if (ec) {
                std::println(std::cerr, "Warning: Failed to remove temporary directory {}: {}", path.string(), ec.message());
            }
        }
    }
    TempTestDir(const TempTestDir&) = delete;
    TempTestDir& operator=(const TempTestDir&) = delete;
};

std::optional<mode_t> get_posix_mode_from_path(const std::filesystem::path& p) {
    struct stat statbuf;
    if (::stat(p.c_str(), &statbuf) != 0) { // stat follows symlinks, which is fine for a dir
        std::println(std::cerr, "Debug: get_posix_mode_from_path for '{}' failed: {}", p.string(), strerror(errno));
        return std::nullopt;
    }
    return statbuf.st_mode;
}

// Helper to create perms from octal for test cases
std::filesystem::perms perms_from_octal(unsigned int octal_val) {
    return static_cast<std::filesystem::perms>(octal_val);
}

// Helper to check if actual POSIX mode reflects the intended std::filesystem::perms
// This focuses on the permission bits typically managed by chmod (07777)
bool check_perms_match(mode_t actual_posix_mode, std::filesystem::perms expected_fs_perms) {
    mode_t expected_posix_mask = 0;
    if ((expected_fs_perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none) expected_posix_mask |= S_IRUSR;
    if ((expected_fs_perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none) expected_posix_mask |= S_IWUSR;
    if ((expected_fs_perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) expected_posix_mask |= S_IXUSR;
    if ((expected_fs_perms & std::filesystem::perms::group_read) != std::filesystem::perms::none) expected_posix_mask |= S_IRGRP;
    if ((expected_fs_perms & std::filesystem::perms::group_write) != std::filesystem::perms::none) expected_posix_mask |= S_IWGRP;
    if ((expected_fs_perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none) expected_posix_mask |= S_IXGRP;
    if ((expected_fs_perms & std::filesystem::perms::others_read) != std::filesystem::perms::none) expected_posix_mask |= S_IROTH;
    if ((expected_fs_perms & std::filesystem::perms::others_write) != std::filesystem::perms::none) expected_posix_mask |= S_IWOTH;
    if ((expected_fs_perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none) expected_posix_mask |= S_IXOTH;
    if ((expected_fs_perms & std::filesystem::perms::set_uid) != std::filesystem::perms::none) expected_posix_mask |= S_ISUID;
    if ((expected_fs_perms & std::filesystem::perms::set_gid) != std::filesystem::perms::none) expected_posix_mask |= S_ISGID;
    if ((expected_fs_perms & std::filesystem::perms::sticky_bit) != std::filesystem::perms::none) expected_posix_mask |= S_ISVTX;

    // Compare the relevant permission bits (07777 part of the mode)
    return (actual_posix_mode & 07777) == expected_posix_mask;
}
// --- End of Helper code ---

struct CreateDirsTestCase {
    std::string name;
    std::string path_to_create_suffix;
    std::filesystem::perms perms_for_final_dir;
    xinim::fs::filesystem_ops::mode op_mode; // For the change_permissions call within create_directories_hybrid
    bool expect_success;
    std::optional<std::errc> expected_ec_val_on_error;

    void run(xinim::fs::filesystem_ops& fs_ops, const std::filesystem::path& base_path, int& failures) const {
        std::filesystem::path full_path_to_create = base_path / path_to_create_suffix;
        std::print(std::cout, "Test Case: {} (Path: '{}', Mode: {})... ",
                   name, full_path_to_create.string(),
                   op_mode == xinim::fs::filesystem_ops::mode::standard ? "standard" : "direct");

        // Ensure the specific test path is clean before running, but not the base_path itself here.
        std::error_code rm_ec;
        std::filesystem::remove_all(full_path_to_create, rm_ec); // Clean slate for this specific path suffix

        auto result = fs_ops.create_directories_hybrid(full_path_to_create, perms_for_final_dir, op_mode);

        if (result.has_value()) {
            if (expect_success) {
                if (!std::filesystem::is_directory(full_path_to_create)) {
                    std::println(std::cout, "FAIL (path is not a directory after creation)");
                    failures++;
                    return;
                }
                auto current_mode_opt = get_posix_mode_from_path(full_path_to_create);
                if (!current_mode_opt) {
                     std::println(std::cout, "FAIL (could not stat created directory '{}' to check perms)", full_path_to_create.string());
                     failures++;
                     return;
                }
                // Check permissions of the final directory component
                if (check_perms_match(*current_mode_opt, perms_for_final_dir)) {
                    std::println(std::cout, "PASS");
                } else {
                    std::println(std::cout, "FAIL (final permissions not set as expected. Got mode 0{:o}, expected perms equivalent to 0{:o})", *current_mode_opt & 07777, static_cast<unsigned int>(perms_for_final_dir) );
                    failures++;
                }
            } else {
                std::println(std::cout, "FAIL (expected error, got success)");
                failures++;
            }
        } else { // Error case
            if (expect_success) {
                std::println(std::cout, "FAIL (expected success, got error: {})", result.error().message());
                failures++;
            } else {
                bool error_code_matched = false;
                if (expected_ec_val_on_error) {
                    if (result.error() == *expected_ec_val_on_error) { // Compare std::error_code
                        error_code_matched = true;
                    }
                } else { // Any error was expected
                    error_code_matched = true;
                }

                if (error_code_matched) {
                    std::println(std::cout, "PASS (got expected error: {})", result.error().message());
                } else {
                    std::println(std::cout, "FAIL");
                    std::println(std::cerr, "  Expected error code: {}, Got: {} ({})",
                                 std::make_error_code(*expected_ec_val_on_error).message(),
                                 result.error().value(), result.error().message());
                    failures++;
                }
            }
        }
    }
};


int main() {
    xinim::fs::filesystem_ops fs_ops;
    int failures = 0;
    srand(static_cast<unsigned int>(time(nullptr)));

    // Base temporary directory for all tests in this run. This one *is* created.
    TempTestDir main_test_env_base("CreateDirsHybridTestEnv");
    std::error_code ec_main_base;
    std::filesystem::create_directory(main_test_env_base.path, ec_main_base);
    if (ec_main_base) {
        std::println(std::cerr, "FATAL: Could not create main test environment base directory: {}", ec_main_base.message());
        return EXIT_FAILURE;
    }


    std::vector<CreateDirsTestCase> test_cases = {
        {"NewPath_StdMode_Perms755", "a/b/c", perms_from_octal(0755), xinim::fs::filesystem_ops::mode::standard, true, {}},
        {"NewPath_DirectMode_Perms700", "d/e/f", perms_from_octal(0700), xinim::fs::filesystem_ops::mode::direct, true, {}},
        // Test relies on previous test to create "a/b/c". This makes tests dependent.
        // Better to make each test setup its specific pre-conditions or use unique paths.
        // For this one, we'll test creating an already existing path.
        {"ExistingPath_StdMode_Perms777", "a/b/c", perms_from_octal(0777), xinim::fs::filesystem_ops::mode::standard, true, {}},
        {"ParentExists_DirectMode_Perms750", "d/e/f/g", perms_from_octal(0750), xinim::fs::filesystem_ops::mode::direct, true, {}},
    };

    for (const auto& tc : test_cases) {
        // Each test case gets its path constructed relative to the main_test_env_base.path
        tc.run(fs_ops, main_test_env_base.path, failures);
    }

    // Test case: file blocks path
    { // New scope for this TempTestDir to manage its own base
        TempTestDir file_block_base_env("FileBlockBase");
        std::filesystem::create_directory(file_block_base_env.path, ec_main_base); // Create this sub-base
         if (ec_main_base) {
            std::println(std::cerr, "FATAL: Could not create file_block_base_env directory: {}", ec_main_base.message());
            failures++; // Count as failure
        } else {
            std::filesystem::path parent_for_file = file_block_base_env.path / "parent_ok";
            std::filesystem::path blocking_file = parent_for_file / "blocking_file_component";

            std::filesystem::create_directory(parent_for_file); // Create parent
            { std::ofstream f(blocking_file); f << "block"; } // Create blocking file

            CreateDirsTestCase file_block_tc = {
                "FileBlocksPath_StdMode",
                (parent_for_file.lexically_relative(file_block_base_env.path) / "blocking_file_component" / "new_dir").string(),
                perms_from_octal(0755), xinim::fs::filesystem_ops::mode::standard, false, std::errc::file_exists // Or not_a_directory from pre-check
            };
            file_block_tc.run(fs_ops, file_block_base_env.path, failures);
        }
    }


    if (failures > 0) {
        std::println(std::cerr, "\n{} XINIM::FS::CREATE_DIRECTORIES_HYBRID TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::CREATE_DIRECTORIES_HYBRID TESTS PASSED.");
    return EXIT_SUCCESS;
}
