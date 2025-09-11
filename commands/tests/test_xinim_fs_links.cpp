// commands/tests/test_xinim_fs_links.cpp
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
#include <chrono>       // For unique name generation
#include <functional>   // For std::function

// POSIX includes for creating test conditions and verifying
#include <sys/stat.h>   // For ::stat, ::lstat
#include <unistd.h>     // For ::symlink, ::link (POSIX)
#include <ctime>        // For time for srand
#include <cstring>      // For strerror


// --- Start of Helper code (adapted from previous tests) ---
namespace { // Anonymous namespace for test helpers

class TempTestEntity {
public:
    std::filesystem::path path;
    enum class EntityType { File, Directory, Symlink };
    EntityType type_managed; // The type this TempTestEntity instance represents or manages path for

    TempTestEntity(const std::string& base_name_prefix, EntityType type,
                   const std::filesystem::path& target_for_symlink = "", bool auto_create = true) {
        type_managed = type;

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
                if (target_for_symlink.empty()) {
                     std::println(std::cerr, "FATAL: Symlink target must be provided for auto-creating symlink TempTestEntity.");
                     std::exit(EXIT_FAILURE);
                }
                std::filesystem::create_symlink(target_for_symlink, path, ec_setup);
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

struct LinkTestCase {
    std::string name;
    std::string target_spec_str;
    std::string link_spec_str;

    enum class Operation { CreateSymlink, CreateHardlink, ReadSymlink };
    Operation op_type;
    xinim::fs::mode op_mode_for_ctx;

    bool expect_success;
    std::optional<std::errc> expected_ec_val_on_error;
    std::string expected_readlink_target_content;

    std::function<void(const std::filesystem::path& /*link_path*/, const std::filesystem::path& /*target_path*/)> setup_action = nullptr;

    void run(const std::filesystem::path& test_run_base_path, // Base for creating actual links for this test run
             const std::filesystem::path& default_target_setup_base_path, // Base for resolving relative target_spec_str
             int& failures) const {

        xinim::fs::operation_context ctx;
        ctx.execution_mode = op_mode_for_ctx;
        // ctx.follow_symlinks is not directly used by create_symlink/create_hard_link/read_symlink's own logic,
        // but could be if they called other xinim::fs functions that use it. Default is fine.

        std::print(std::cout, "Test Case: {} (Op: {}, Mode: {})... ",
                   name,
                   (op_type == Operation::CreateSymlink ? "CreateSym" : (op_type == Operation::CreateHardlink ? "CreateHard" : "ReadSym")),
                   (ctx.execution_mode == xinim::fs::mode::standard ? "standard" : "direct"));

        std::filesystem::path full_link_path = test_run_base_path / link_spec_str;
        std::filesystem::path full_target_path;

        if (!target_spec_str.empty()) {
            std::filesystem::path p_target_spec(target_spec_str);
            if (p_target_spec.is_absolute()) {
                full_target_path = p_target_spec;
            } else {
                full_target_path = default_target_setup_base_path / target_spec_str;
            }
        }

        // Ensure clean state for link path before creation tests
        if (op_type == Operation::CreateSymlink || op_type == Operation::CreateHardlink) {
             std::error_code rm_ec; std::filesystem::remove(full_link_path, rm_ec); // Ignore error if not found
        }

        if (setup_action) {
            setup_action(full_link_path, full_target_path);
        }

        std::expected<void, std::error_code> create_result;
        std::expected<std::filesystem::path, std::error_code> read_result;
        bool actual_success = false;
        std::error_code actual_ec;

        switch (op_type) {
            case Operation::CreateSymlink:
                create_result = xinim::fs::create_symlink(full_target_path, full_link_path, ctx);
                actual_success = create_result.has_value();
                actual_ec = create_result ? std::error_code{} : create_result.error();
                break;
            case Operation::CreateHardlink:
                create_result = xinim::fs::create_hard_link(full_target_path, full_link_path, ctx);
                actual_success = create_result.has_value();
                actual_ec = create_result ? std::error_code{} : create_result.error();
                break;
            case Operation::ReadSymlink: // Link should exist due to test setup
                read_result = xinim::fs::read_symlink(full_link_path, ctx);
                actual_success = read_result.has_value();
                actual_ec = read_result ? std::error_code{} : read_result.error();
                break;
        }

        if (actual_success) {
            if (expect_success) {
                bool verification_passed = true;
                if (op_type == Operation::CreateSymlink) {
                    if (!std::filesystem::is_symlink(full_link_path)) {
                        std::println(std::cerr, "\n  Verification FAIL: Link is not a symlink: {}", full_link_path.string()); verification_passed = false;
                    } else {
                        std::error_code ec_read;
                        auto pointed_target = std::filesystem::read_symlink(full_link_path, ec_read);
                        if (ec_read || pointed_target.string() != full_target_path.string()) { // Compare string representations
                             std::println(std::cerr, "\n  Verification FAIL: Symlink target mismatch. Expected '{}', Got '{}' (Read Error: {})",
                                          full_target_path.string(), pointed_target.string(), ec_read.message());
                             verification_passed = false;
                        }
                    }
                } else if (op_type == Operation::CreateHardlink) {
                    std::error_code ec_exists;
                    if (!std::filesystem::exists(full_link_path, ec_exists) || std::filesystem::is_symlink(full_link_path, ec_exists)) { // Hardlink must exist and not be a symlink
                        std::println(std::cerr, "\n  Verification FAIL: Hard link not created or is a symlink: {}", full_link_path.string()); verification_passed = false;
                    } else {
                        auto inode_target = get_inode(full_target_path);
                        auto inode_link = get_inode(full_link_path);
                        if (!inode_target || !inode_link || *inode_target != *inode_link) {
                            std::println(std::cerr, "\n  Verification FAIL: Inodes do not match for hard link. Target ({}): {}, Link ({}): {}",
                                full_target_path.string(), (inode_target ? std::to_string(*inode_target) : "N/A"),
                                full_link_path.string(), (inode_link ? std::to_string(*inode_link) : "N/A"));
                            verification_passed = false;
                        }
                    }
                } else if (op_type == Operation::ReadSymlink) {
                    if (read_result.value().string() != expected_readlink_target_content) {
                        std::println(std::cerr, "\n  Verification FAIL: Read symlink target mismatch. Expected '{}', Got '{}'",
                                     expected_readlink_target_content, read_result.value().string());
                        verification_passed = false;
                    }
                }
                std::println(std::cout, verification_passed ? "PASS" : "FAIL (Verification)");
                if (!verification_passed) failures++;
            } else {
                std::println(std::cout, "FAIL (expected error, got success)"); failures++;
            }
        } else { // Error case
            if (expect_success) {
                std::println(std::cout, "FAIL (expected success, got error: {})", actual_ec.message()); failures++;
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
                    std::println(std::cout, "FAIL (Error mismatch. Expected: {}, Got: {} ({}))",
                                 expected_ec_val_on_error ? std::make_error_code(*expected_ec_val_on_error).message() : "any error",
                                 actual_ec.value(), actual_ec.message());
                    failures++;
                }
            }
        }
        // Cleanup the created link for next test, target is managed by its own TempTestEntity or setup
        if (std::filesystem::is_symlink(full_link_path) || std::filesystem::exists(full_link_path)) {
             std::filesystem::remove(full_link_path); // Use remove for files/symlinks, remove_all for dirs
        }
    }
};


int main() {
    int failures = 0;
    srand(static_cast<unsigned int>(time(nullptr)));

    TempTestEntity test_run_base("LinkTestRunBase", TempTestEntity::EntityType::Directory);

    TempTestEntity target_file_entity("global_target_file.txt", TempTestEntity::EntityType::File);
    TempTestEntity target_dir_entity("global_target_dir", TempTestEntity::EntityType::Directory);
    std::filesystem::path non_existent_target_for_symlink = test_run_base.path / "no_such_actual_target";

    // Pre-create symlinks for read tests
    std::filesystem::path sym_to_file_for_read = test_run_base.path / "s_to_file_for_read.lnk";
    std::filesystem::path sym_to_dir_for_read  = test_run_base.path / "s_to_dir_for_read.lnk";
    std::filesystem::path dangling_sym_for_read = test_run_base.path / "s_dangling_for_read.lnk";

    std::filesystem::create_symlink(target_file_entity.path, sym_to_file_for_read);
    std::filesystem::create_directory_symlink(target_dir_entity.path, sym_to_dir_for_read);
    std::filesystem::create_symlink(non_existent_target_for_symlink, dangling_sym_for_read);


    std::vector<LinkTestCase> test_cases = {
        // CreateSymlink
        {"CreateSym_File_Std", target_file_entity.path.string(), "s_file_std.lnk", LinkTestCase::Operation::CreateSymlink, xinim::fs::mode::standard, true, {}, ""},
        {"CreateSym_File_Direct", target_file_entity.path.string(), "s_file_direct.lnk", LinkTestCase::Operation::CreateSymlink, xinim::fs::mode::direct, true, {}, ""},
        {"CreateSym_ToDir_Std", target_dir_entity.path.string(), "s_dir_std.lnk", LinkTestCase::Operation::CreateSymlink, xinim::fs::mode::standard, true, {}, ""},
        {"CreateSym_ToNonExistTarget_Std", non_existent_target_for_symlink.string(), "s_nonexist_std.lnk", LinkTestCase::Operation::CreateSymlink, xinim::fs::mode::standard, true, {}, ""},
        {"CreateSym_LinkExistsAsFile_Std_Fails", target_file_entity.path.string(), "s_exists_file.txt", LinkTestCase::Operation::CreateSymlink, xinim::fs::mode::standard, false, std::errc::file_exists, "",
            [](const auto& link_p, const auto&){ std::ofstream f(link_p); f << "exists"; }},

        // CreateHardlink
        {"CreateHard_File_Std", target_file_entity.path.string(), "h_file_std.lnk", LinkTestCase::Operation::CreateHardlink, xinim::fs::mode::standard, true, {}, ""},
        {"CreateHard_File_Direct", target_file_entity.path.string(), "h_file_direct.lnk", LinkTestCase::Operation::CreateHardlink, xinim::fs::mode::direct, true, {}, ""},
        {"CreateHard_ToDir_Std_Fails", target_dir_entity.path.string(), "h_dir_std.lnk", LinkTestCase::Operation::CreateHardlink, xinim::fs::mode::standard, false, std::errc::operation_not_permitted, ""},
        {"CreateHard_NonExistTarget_Std_Fails", non_existent_target_for_symlink.string(), "h_nonexist_std.lnk", LinkTestCase::Operation::CreateHardlink, xinim::fs::mode::standard, false, std::errc::no_such_file_or_directory, ""},
        {"CreateHard_LinkExistsAsFile_Std_Fails", target_file_entity.path.string(), "h_exists_file.txt", LinkTestCase::Operation::CreateHardlink, xinim::fs::mode::standard, false, std::errc::file_exists, "",
            [](const auto& link_p, const auto&){ std::ofstream f(link_p); f << "exists"; }},

        // ReadSymlink
        {"ReadSym_ToFile_Std", "", sym_to_file_for_read.lexically_relative(test_run_base.path).string(), LinkTestCase::Operation::ReadSymlink, xinim::fs::mode::standard, true, {}, target_file_entity.path.string()},
        {"ReadSym_ToDir_Std", "", sym_to_dir_for_read.lexically_relative(test_run_base.path).string(), LinkTestCase::Operation::ReadSymlink, xinim::fs::mode::standard, true, {}, target_dir_entity.path.string()},
        {"ReadSym_Dangling_Std", "", dangling_sym_for_read.lexically_relative(test_run_base.path).string(), LinkTestCase::Operation::ReadSymlink, xinim::fs::mode::standard, true, {}, non_existent_target_for_symlink.string()},
        {"ReadSym_NotASymlink_Std_Fails", "", target_file_entity.path.lexically_relative(test_run_base.path).string(), LinkTestCase::Operation::ReadSymlink, xinim::fs::mode::standard, false, std::errc::invalid_argument, ""},
        {"ReadSym_NonExistentPath_Std_Fails", "", "non_existent_symlink_file.lnk", LinkTestCase::Operation::ReadSymlink, xinim::fs::mode::standard, false, std::errc::no_such_file_or_directory, ""},
    };

    for (const auto& tc : test_cases) {
        tc.run(test_run_base.path, test_run_base.path, failures);
    }

    // Cleanup for symlinks created for read tests
    std::error_code ec_cleanup;
    std::filesystem::remove(sym_to_file_for_read, ec_cleanup);
    std::filesystem::remove(sym_to_dir_for_read, ec_cleanup);
    std::filesystem::remove(dangling_sym_for_read, ec_cleanup);

    if (failures > 0) {
        std::println(std::cerr, "\n{} XINIM::FS::LINK TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::LINK TESTS PASSED.");
    return EXIT_SUCCESS;
}
