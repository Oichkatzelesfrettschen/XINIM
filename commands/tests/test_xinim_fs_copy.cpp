// commands/tests/test_xinim_fs_copy.cpp
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
#include <algorithm> // For std::equal, std::sort
#include <iterator>  // For std::istreambuf_iterator
#include <chrono>    // For unique name generation
#include <functional>// For std::function

// POSIX includes for creating test conditions and verifying
#include <sys/stat.h> // Not strictly needed for copy_file tests, but often in test utils
#include <unistd.h>   // Not strictly needed
#include <ctime>      // For time for srand
#include <cstring>    // For strerror


// --- Start of Helper code (adapted from previous tests) ---
namespace { // Anonymous namespace for test helpers

class TempTestEntity {
public:
    std::filesystem::path path;
    enum class EntityType { File, Directory, Symlink };

    TempTestEntity(const std::filesystem::path& base_dir, const std::string& name_prefix,
                   EntityType type = EntityType::File,
                   const std::string& content = "default_content",
                   const std::filesystem::path& symlink_target = "",
                   bool auto_create = true) {

        auto now_chrono = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now_chrono.time_since_epoch()).count();
        static int counter = 0;
        std::string unique_suffix = "_" + std::to_string(nanos) + "_" + std::to_string(counter++);
        // Ensure the path is constructed relative to the provided base_dir
        path = base_dir / (name_prefix + unique_suffix);

        if (auto_create) {
            std::error_code ec_setup;
            if (type == EntityType::Directory) {
                std::filesystem::create_directory(path, ec_setup);
            } else if (type == EntityType::File) {
                std::ofstream outfile(path, std::ios::binary);
                if (outfile) { outfile << content; }
                else { ec_setup = std::make_error_code(std::errc::io_error); }
            } else if (type == EntityType::Symlink) {
                if (symlink_target.empty()) { throw std::runtime_error("Symlink target needed for auto_create");}
                std::filesystem::create_symlink(symlink_target, path, ec_setup);
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
            // Log if checking existence itself failed for an unexpected reason
             std::println(std::cerr, "Warning: Failed to check existence of temporary entity {}: {}", path.string(), ec_exists.message());
        }
    }
    TempTestEntity(const TempTestEntity&) = delete;
    TempTestEntity& operator=(const TempTestEntity&) = delete;

    void create_file(const std::string& name, const std::string& content = "dummy") {
        if (!std::filesystem::is_directory(path)) throw std::runtime_error("Base path is not a directory for create_file");
        std::ofstream f(path / name); f << content;
    }
    void create_dir(const std::string& name) {
        if (!std::filesystem::is_directory(path)) throw std::runtime_error("Base path is not a directory for create_dir");
        std::filesystem::create_directory(path / name);
    }
    void create_symlink_inside(const std::string& link_name, const std::filesystem::path& target) {
         if (!std::filesystem::is_directory(path)) throw std::runtime_error("Base path is not a directory for create_symlink_inside");
         std::filesystem::create_symlink(target, path / link_name);
    }
};

bool compare_file_contents(const std::filesystem::path& p1, const std::filesystem::path& p2) {
    std::error_code ec1, ec2;
    bool p1_exists = std::filesystem::exists(p1, ec1);
    bool p2_exists = std::filesystem::exists(p2, ec2);

    if (!p1_exists || !p2_exists) {
        std::println(std::cerr, "compare_file_contents: One or both files do not exist. P1({}): {}, P2({}): {}", p1.string(), p1_exists, p2.string(), p2_exists);
        return false;
    }
    if (ec1 || ec2) { // Error checking existence
        std::println(std::cerr, "compare_file_contents: Error checking existence. P1_ec: {}, P2_ec: {}", ec1.message(), ec2.message());
        return false;
    }

    auto size1 = std::filesystem::file_size(p1, ec1);
    auto size2 = std::filesystem::file_size(p2, ec2);
    if (ec1 || ec2 || size1 != size2) {
        std::println(std::cerr, "compare_file_contents: Files differ in size or error getting size. P1({}): {}, P2({}): {}", p1.string(), size1, p2.string(), size2);
        return false;
    }
    if (size1 == 0) return true;

    std::ifstream f1(p1, std::ios::binary);
    std::ifstream f2(p2, std::ios::binary);
    if (!f1.is_open()) {std::println(std::cerr, "compare_file_contents: Could not open file: '{}'", p1.string()); return false;}
    if (!f2.is_open()) {std::println(std::cerr, "compare_file_contents: Could not open file: '{}'", p2.string()); return false;}

    bool are_equal = std::equal(std::istreambuf_iterator<char>(f1), std::istreambuf_iterator<char>(),
                                std::istreambuf_iterator<char>(f2), std::istreambuf_iterator<char>());
    if (!are_equal) {
        std::println(std::cerr, "compare_file_contents: Files '{}' and '{}' differ in content.", p1.string(), p2.string());
    }
    return are_equal;
}

bool verify_dir_structure(const std::filesystem::path& dir_path, const std::vector<std::string>& expected_children_names) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir_path, ec) || ec) return false;

    std::vector<std::string> found_children;
    for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        if (ec) return false; // Error iterating
        found_children.push_back(entry.path().filename().string());
    }
    std::sort(found_children.begin(), found_children.end());
    std::vector<std::string> sorted_expected = expected_children_names;
    std::sort(sorted_expected.begin(), sorted_expected.end());

    if (found_children != sorted_expected) {
        std::println(std::cerr, "verify_dir_structure: Mismatch for '{}'. Expected: [{}], Got: [{}]",
            dir_path.string(), fmt::join(sorted_expected, ", "), fmt::join(found_children, ", "));
        return false;
    }
    return true;
}

}
// --- End of Helper code ---

struct CopyTestCase {
    std::string name;
    TempTestEntity::EntityType source_type;
    std::string source_content_or_target_str;
    std::vector<std::pair<std::string, std::string>> source_dir_children_files;
    std::vector<std::string> source_dir_children_dirs;
    std::vector<std::pair<std::string, std::string>> source_dir_children_symlinks;

    std::string dest_suffix;
    bool dest_pre_exists;
    TempTestEntity::EntityType dest_pre_existing_type;
    std::string dest_pre_existing_content;

    std::filesystem::copy_options options;
    xinim::fs::operation_context ctx_params;

    bool expect_success;
    std::optional<std::errc> expected_ec_val_on_error;

    std::function<void(const std::filesystem::path& /*source_path_to_be_created*/)> setup_source_action = nullptr;


    void run(const std::filesystem::path& test_case_base_path, int& failures) const {
        std::print(std::cout, "Test Case: {} ... ", name);
        // Mode and follow_symlinks from ctx_params can be printed for more detail if needed.

        TempTestEntity source_holder(test_case_base_path, name + "_source_base", TempTestEntity::EntityType::Directory, "", "", true);
        std::filesystem::path full_source_path;

        if (source_type == TempTestEntity::EntityType::File) {
            full_source_path = source_holder.path / "source_file.dat";
            std::ofstream f(full_source_path, std::ios::binary); f << source_content_or_target_str;
        } else if (source_type == TempTestEntity::EntityType::Directory) {
            full_source_path = source_holder.path / "source_dir";
            std::filesystem::create_directory(full_source_path);
            for(const auto& child_f : source_dir_children_files) { std::ofstream f(full_source_path / child_f.first, std::ios::binary); f << child_f.second;}
            for(const auto& child_d : source_dir_children_dirs) { std::filesystem::create_directory(full_source_path / child_d); }
            for(const auto& child_s : source_dir_children_symlinks) { std::filesystem::create_symlink(child_s.second, full_source_path / child_s.first); }
        } else { // Symlink
            full_source_path = source_holder.path / "source.symlink";
            std::filesystem::create_symlink(source_content_or_target_str, full_source_path);
        }

        if (setup_source_action) { // For special cases like non-existent source
            setup_source_action(full_source_path);
        }

        std::filesystem::path full_dest_path = test_case_base_path / dest_suffix;
        std::filesystem::remove_all(full_dest_path);
        if (dest_pre_exists) {
            if (dest_pre_existing_type == TempTestEntity::EntityType::Directory) std::filesystem::create_directory(full_dest_path);
            else { std::ofstream f(full_dest_path, std::ios::binary); f << dest_pre_existing_content; }
        }

        auto result = xinim::fs::copy(full_source_path, full_dest_path, options, ctx_params);
        bool actual_op_succeeded = result.has_value();
        std::error_code actual_ec = result ? std::error_code{} : result.error();

        if (actual_op_succeeded) {
            if (expect_success) {
                bool post_check_passed = true;
                std::error_code ec_dest_exists;
                auto dest_status_after_copy = std::filesystem::symlink_status(full_dest_path, ec_dest_exists);

                if (!std::filesystem::exists(dest_status_after_copy) || ec_dest_exists) {
                    std::println(std::cerr, "\n  Verification FAIL: Dest path '{}' does not exist or error stating it. EC: {}", full_dest_path.string(), ec_dest_exists.message()); post_check_passed = false;
                } else {
                    // Determine what the 'from' type effectively was for the copy operation
                    xinim::fs::operation_context from_stat_ctx = ctx_params;
                    from_stat_ctx.follow_symlinks = !(options & std::filesystem::copy_options::copy_symlinks);
                    auto from_effective_status_res = xinim::fs::get_status(full_source_path, from_stat_ctx);

                    if(!from_effective_status_res && source_type != TempTestEntity::EntityType::Symlink && !(options & std::filesystem::copy_options::copy_symlinks)) {
                        // If source was not a symlink to be copied as link, and we can't stat it (e.g. dangling symlink target)
                         std::println(std::cerr, "\n  Verification INFO: Could not get effective status of source '{}' for detailed type check.", full_source_path.string());
                    } else if (from_effective_status_res) {
                        const auto& from_effective_stat = from_effective_status_res.value();
                        if (from_effective_stat.type == std::filesystem::file_type::regular) {
                            if (!std::filesystem::is_regular_file(dest_status_after_copy)) { std::println(std::cerr, "\n  Verification FAIL: Dest not regular file."); post_check_passed = false;}
                            else if (!compare_file_contents(full_source_path, full_dest_path)) { std::println(std::cerr, "\n  Verification FAIL: File contents differ."); post_check_passed = false;}
                        } else if (from_effective_stat.type == std::filesystem::file_type::directory) {
                            if (!std::filesystem::is_directory(dest_status_after_copy)) {std::println(std::cerr, "\n  Verification FAIL: Dest not directory."); post_check_passed = false;}
                            else if (options & std::filesystem::copy_options::recursive) {
                                std::vector<std::string> expected_children_names;
                                for(const auto& child_f : source_dir_children_files) expected_children_names.push_back(child_f.first);
                                for(const auto& child_d : source_dir_children_dirs) expected_children_names.push_back(child_d);
                                for(const auto& child_s : source_dir_children_symlinks) expected_children_names.push_back(child_s.first);
                                if (!verify_dir_structure(full_dest_path, expected_children_names)) {std::println(std::cerr, "\n  Verification FAIL: Dir structure mismatch."); post_check_passed = false;}
                            }
                        }
                    }
                    // If original 'from' was a symlink and copy_symlinks was used
                    if (source_type == TempTestEntity::EntityType::Symlink && (options & std::filesystem::copy_options::copy_symlinks)) {
                        if (!std::filesystem::is_symlink(dest_status_after_copy)) {std::println(std::cerr, "\n  Verification FAIL: Dest not symlink when copy_symlinks set."); post_check_passed = false;}
                        else if (std::filesystem::read_symlink(full_dest_path) != source_content_or_target_str) {std::println(std::cerr, "\n  Verification FAIL: Symlink target content mismatch."); post_check_passed = false;}
                    }
                }
                std::println(std::cout, post_check_passed ? "PASS" : "FAIL (Post-conditions)");
                if(!post_check_passed) failures++;
            } else { std::println(std::cout, "FAIL (expected error, got success)"); failures++; }
        } else { /* Error case */
            if (expect_success) { std::println(std::cout, "FAIL (expected success, got error: {})", actual_ec.message()); failures++;}
            else {
                if (expected_ec_on_error_val && actual_ec == *expected_ec_on_error_val) { std::println(std::cout, "PASS (got expected error: {})", actual_ec.message()); }
                else if (!expected_ec_on_error_val && actual_ec) { std::println(std::cout, "PASS (any error was expected and got: {})", actual_ec.message());}
                else { std::println(std::cout, "FAIL (Error mismatch. Expected: {}, Got: {} ({}))", expected_ec_on_error_val ? std::make_error_code(*expected_ec_on_error_val).message() : "any error", actual_ec.value(), actual_ec.message()); failures++;}
            }
        }
        std::filesystem::remove_all(full_dest_path);
    }
};


int main() {
    int failures = 0;
    srand(static_cast<unsigned int>(time(nullptr)));
    xinim::fs::operation_context default_ctx; // For tests not varying context deeply

    TempTestEntity test_run_base_dir("CopyMainTestRunBase", TempTestEntity::EntityType::Directory, "", true);

    TempTestEntity main_src_file_entity(test_run_base_dir.path, "main_src_file", TempTestEntity::EntityType::File, "main_copy_content_for_symlink_target");


    std::vector<CopyTestCase> test_cases = {
        {"CopyFile_New_Std", TempTestEntity::EntityType::File, "file_A_content", {}, {}, {}, "dest_A_std.txt", false, {}, "", std::filesystem::copy_options::none, default_ctx, true, {}},
        {"CopyFile_Overwrite_Std", TempTestEntity::EntityType::File, "file_B_new_content", {}, {}, {}, "dest_B_overwrite.txt", true, TempTestEntity::EntityType::File, "old_B_content", std::filesystem::copy_options::overwrite_existing, default_ctx, true, {}},
        {"CopyFile_SkipExisting_Std", TempTestEntity::EntityType::File, "file_C_new_content_skip", {}, {}, {}, "dest_C_skip.txt", true, TempTestEntity::EntityType::File, "PRESERVED_C", std::filesystem::copy_options::skip_existing, default_ctx, true, {}},
        {"CopyFile_FailIfExists_Std", TempTestEntity::EntityType::File, "file_D_content", {}, {}, {}, "dest_D_fail.txt", true, TempTestEntity::EntityType::File, "existing_D_content", std::filesystem::copy_options::none, default_ctx, false, std::errc::file_exists},
        {"CopyDir_NonRecursive_Fails", TempTestEntity::EntityType::Directory, "", {{"f1.txt","c1"}}, {}, {}, "dest_E_dir_nonrec", false, {}, "", std::filesystem::copy_options::none, default_ctx, false, std::errc::is_a_directory},
        {"CopyDir_Recursive_Std", TempTestEntity::EntityType::Directory, "", {{"f1.txt","c1"}}, {"sub1"}, {}, "dest_F_dir_rec", false, {}, "", std::filesystem::copy_options::recursive, default_ctx, true, {}},
        {"CopySymlink_AsLink_Std", TempTestEntity::EntityType::Symlink, main_src_file_entity.path.string(), {}, {}, {}, "dest_G_symlink_as_link.lnk", false, {}, "", std::filesystem::copy_options::copy_symlinks, default_ctx, true, {}},
        {"CopySymlink_AsTarget_Std", TempTestEntity::EntityType::Symlink, main_src_file_entity.path.string(), {}, {}, {}, "dest_H_symlink_as_target.file", false, {}, "", std::filesystem::copy_options::none, default_ctx, true, {}}, // Follows, copies file
        {"CopyNonExistentSource_Std_Fails", TempTestEntity::EntityType::File, "Source NE", {}, {}, {}, "dest_I_nonexist.txt", false, {},"", std::filesystem::copy_options::none, default_ctx, false, std::errc::no_such_file_or_directory,
            [](const auto& src_p){ if(std::filesystem::exists(src_p)) std::filesystem::remove(src_p); }},
    };

    for (const auto& tc : test_cases) {
        tc.run(test_run_base_dir.path, failures);
    }

    if (failures > 0) {
        std::println(std::cerr, "\n{} XINIM::FS::COPY (MAIN) TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::COPY (MAIN) TESTS PASSED.");
    return EXIT_SUCCESS;
}
