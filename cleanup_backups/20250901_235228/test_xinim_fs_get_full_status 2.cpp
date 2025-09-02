// commands/tests/test_xinim_fs_get_full_status.cpp
#include "xinim/filesystem.hpp" // Adjust path as needed
#include <filesystem>
#include <iostream>
#include <fstream>      // For std::ofstream to create files
#include <string>
#include <vector>
#include <cstdlib>      // For EXIT_SUCCESS, EXIT_FAILURE, rand, srand
#include <system_error> // For std::error_code, std::errc
#include <print>        // For std::println (C++23)
#include <chrono>       // For std::chrono::*
#include <thread>       // For std::this_thread::sleep_for
#include <optional>     // For std::optional

// POSIX includes for creating test conditions and verifying
#include <sys/stat.h>   // For ::stat, ::lstat, mode_t constants for verification, S_IS*** macros
#include <unistd.h>     // For ::symlink, ::getuid, ::getgid, ::stat, ::lstat
#include <fcntl.h>      // For open, O_CREAT (not directly used, but related to file creation)
#include <grp.h>        // For struct group (not directly used, but related to GID)
#include <pwd.h>        // For struct passwd (not directly used, but related to UID)
#include <ctime>        // For time for srand
#include <cstring>      // For strerror


// Helper to manage a temporary test entity (file or directory)
class TempTestEntity {
public:
    std::filesystem::path path;
    enum class EntityType { File, Directory, Symlink };
    EntityType type;
    std::filesystem::path symlink_target_path_internal; // Only if type is Symlink

    TempTestEntity(const std::string& base_name_prefix, EntityType entity_type, const std::filesystem::path& target_for_symlink = "") {
        type = entity_type;
        symlink_target_path_internal = target_for_symlink;

        // Create a more unique name
        auto now_for_name = std::chrono::high_resolution_clock::now();
        auto nanos_for_name = std::chrono::duration_cast<std::chrono::nanoseconds>(now_for_name.time_since_epoch()).count();
        static int creation_counter = 0;
        std::string unique_id = base_name_prefix + "_" + std::to_string(nanos_for_name) + "_" + std::to_string(creation_counter++);
        path = std::filesystem::temp_directory_path() / unique_id;

        std::error_code ec_setup;
        if (type == EntityType::Directory) {
            std::filesystem::create_directory(path, ec_setup);
        } else if (type == EntityType::File) {
            std::ofstream outfile(path);
            if (outfile) {
                outfile << "hello test"; // Initial content, size 10
            } else {
                ec_setup = std::make_error_code(std::errc::io_error);
            }
        } else if (type == EntityType::Symlink) {
            if (symlink_target_path_internal.empty()) {
                 std::println(std::cerr, "FATAL: Symlink target must be provided for TempTestEntity of type Symlink.");
                 std::exit(EXIT_FAILURE);
            }
            // Note: For symlinks, the target's existence is up to the specific test case.
            // This constructor just creates the link.
            std::filesystem::create_symlink(symlink_target_path_internal, path, ec_setup);
        }
        if (ec_setup) {
             std::println(std::cerr, "FATAL: Failed to create temporary entity '{}' (type {}): {}", path.string(), static_cast<int>(type), ec_setup.message());
             std::exit(EXIT_FAILURE);
        }
    }

    ~TempTestEntity() {
        std::error_code ec_remove;
        // Symlinks must be removed with remove, not remove_all if they are dangling.
        // If they point to a directory, remove_all on the link might attempt to recurse into target.
        // Safest is to check symlink status first.
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(path, ec_remove))) { // Check before exists for symlinks
            std::filesystem::remove(path, ec_remove);
        } else if (std::filesystem::exists(path)) {
            std::filesystem::remove_all(path, ec_remove); // Handles both files and dirs
        }
        if (ec_remove) {
            std::println(std::cerr, "Warning: Failed to remove temporary entity {}: {}", path.string(), ec_remove.message());
        }
    }
    TempTestEntity(const TempTestEntity&) = delete;
    TempTestEntity& operator=(const TempTestEntity&) = delete;
};


// Helper to convert POSIX mode_t to std::filesystem::perms (from filesystem_ops.cpp test context)
// This is needed for comparing the 'permissions' field of file_status_ex
namespace xinim::fs {
    std::filesystem::perms posix_mode_to_filesystem_perms(mode_t mode) {
        std::filesystem::perms p = std::filesystem::perms::none;
        if (mode & S_IRUSR) p |= std::filesystem::perms::owner_read; if (mode & S_IWUSR) p |= std::filesystem::perms::owner_write; if (mode & S_IXUSR) p |= std::filesystem::perms::owner_exec;
        if (mode & S_IRGRP) p |= std::filesystem::perms::group_read; if (mode & S_IWGRP) p |= std::filesystem::perms::group_write; if (mode & S_IXGRP) p |= std::filesystem::perms::group_exec;
        if (mode & S_IROTH) p |= std::filesystem::perms::others_read; if (mode & S_IWOTH) p |= std::filesystem::perms::others_write; if (mode & S_IXOTH) p |= std::filesystem::perms::others_exec;
        if (mode & S_ISUID) p |= std::filesystem::perms::set_uid; if (mode & S_ISGID) p |= std::filesystem::perms::set_gid; if (mode & S_ISVTX) p |= std::filesystem::perms::sticky_bit;
        return p;
    }
}


// Function to compare relevant fields of file_status_ex with struct stat
bool compare_status(const xinim::fs::file_status_ex& fs_ex, const struct stat& posix_stat, const std::filesystem::path& original_path_for_error) {
    bool match = true;
    auto print_mismatch = [&](const std::string& field, auto val_ex, auto val_posix){
        std::println(std::cerr, "  Mismatch for '{}': Field: {}, Expected (approx from POSIX): {}, Got (from fs_ex): {}", original_path_for_error.string(), field, val_posix, val_ex);
        match = false;
    };

    if (fs_ex.uid != posix_stat.st_uid) print_mismatch("UID", fs_ex.uid, posix_stat.st_uid);
    if (fs_ex.gid != posix_stat.st_gid) print_mismatch("GID", fs_ex.gid, posix_stat.st_gid);

    // Size comparison is tricky for special files if rdevice is used.
    // For regular files/symlinks (if lstat), st_size is correct. For directories, st_size is system-dependent.
    if (S_ISREG(posix_stat.st_mode) || S_ISLNK(posix_stat.st_mode)) {
       if (fs_ex.file_size != static_cast<std::uintmax_t>(posix_stat.st_size)) print_mismatch("Size", fs_ex.file_size, posix_stat.st_size);
    } else if (S_ISCHR(posix_stat.st_mode) || S_ISBLK(posix_stat.st_mode)) {
        if (fs_ex.rdevice != posix_stat.st_rdev) print_mismatch("RDevice (for size)", fs_ex.rdevice, posix_stat.st_rdev);
    } // Not strictly checking dir size due to variability.

    if (fs_ex.link_count != static_cast<nlink_t>(posix_stat.st_nlink)) print_mismatch("Link count", fs_ex.link_count, static_cast<nlink_t>(posix_stat.st_nlink));
    if (fs_ex.inode != posix_stat.st_ino) print_mismatch("Inode", fs_ex.inode, posix_stat.st_ino);

    // Timestamp checks (within a tolerance of 2 seconds for most systems)
    if (std::abs(std::chrono::system_clock::to_time_t(fs_ex.mtime) - posix_stat.st_mtime) > 2) print_mismatch("Mtime", std::chrono::system_clock::to_time_t(fs_ex.mtime), posix_stat.st_mtime);
    if (std::abs(std::chrono::system_clock::to_time_t(fs_ex.atime) - posix_stat.st_atime) > 2) print_mismatch("Atime", std::chrono::system_clock::to_time_t(fs_ex.atime), posix_stat.st_atime);
    if (std::abs(std::chrono::system_clock::to_time_t(fs_ex.ctime) - posix_stat.st_ctime) > 2) print_mismatch("Ctime", std::chrono::system_clock::to_time_t(fs_ex.ctime), posix_stat.st_ctime);

    mode_t mode = posix_stat.st_mode;
    std::filesystem::file_type expected_type_from_posix = std::filesystem::file_type::unknown;
    if (S_ISREG(mode)) expected_type_from_posix = std::filesystem::file_type::regular;
    else if (S_ISDIR(mode)) expected_type_from_posix = std::filesystem::file_type::directory;
    else if (S_ISLNK(mode)) expected_type_from_posix = std::filesystem::file_type::symlink;
    else if (S_ISBLK(mode)) expected_type_from_posix = std::filesystem::file_type::block;
    else if (S_ISCHR(mode)) expected_type_from_posix = std::filesystem::file_type::character;
    else if (S_ISFIFO(mode)) expected_type_from_posix = std::filesystem::file_type::fifo;
    else if (S_ISSOCK(mode)) expected_type_from_posix = std::filesystem::file_type::socket;
    if (fs_ex.type != expected_type_from_posix) print_mismatch("Type", static_cast<int>(fs_ex.type), static_cast<int>(expected_type_from_posix));

    std::filesystem::perms expected_perms_from_posix = xinim::fs::posix_mode_to_filesystem_perms(posix_stat.st_mode);
    if (fs_ex.permissions != expected_perms_from_posix) print_mismatch("Permissions", static_cast<unsigned int>(fs_ex.permissions), static_cast<unsigned int>(expected_perms_from_posix));

    return match;
}


// Test case structure
struct StatusTestCase {
    std::string name;
    TempTestEntity::EntityType entity_type_to_create;
    std::filesystem::path symlink_target_path_config; // if entity_type_to_create is Symlink
    bool follow_symlinks_param;
    bool expect_success;
    std::optional<std::errc> expected_ec_val_on_error;

    void run(xinim::fs::filesystem_ops& fs_ops, int& failures) const {
        std::print(std::cout, "Test Case: {} (Follow: {})... ", name, follow_symlinks_param);

        std::optional<TempTestEntity> entity_opt;
        std::filesystem::path path_to_test;
        std::filesystem::path effective_symlink_target = symlink_target_path_config; // Can be empty if not a symlink test

        if (name == "NonExistentFile") {
            path_to_test = std::filesystem::temp_directory_path() / "definitely_not_there_xyz123.txt";
            // Ensure it's indeed not there
            std::filesystem::remove(path_to_test);
        } else {
            try {
                // For symlink tests, resolve the actual target path if it's one of our temp entities
                if (entity_type_to_create == TempTestEntity::EntityType::Symlink) {
                     if (symlink_target_path_config.string() == "TARGET_FILE") { // Special marker
                        TempTestEntity target_file_for_link(name + "_target_file", TempTestEntity::EntityType::File);
                        effective_symlink_target = target_file_for_link.path;
                        entity_opt.emplace(name, entity_type_to_create, effective_symlink_target);
                        // target_file_for_link will be cleaned up when entity_opt (which holds the symlink) is destroyed IF it's created inside this block.
                        // This is tricky. For now, create global targets or ensure lifetime.
                        // Let's assume global targets `symlink_target_file_global` and `symlink_target_dir_global` are created in main.
                     } else if (symlink_target_path_config.string() == "TARGET_DIR") {
                        // Similar logic if targeting a temp dir
                     }
                     // If symlink_target_path_config is already absolute or a fixed known path, use it.
                }
                if (!entity_opt) { // If not created via special symlink logic
                   entity_opt.emplace(name, entity_type_to_create, effective_symlink_target);
                }
                path_to_test = entity_opt->path;

            } catch (const std::runtime_error& e) {
                std::println(std::cout, "SKIP (Setup failed: {})", e.what());
                return;
            }
        }

        auto result = fs_ops.get_full_status(path_to_test, follow_symlinks_param);

        if (result.has_value()) {
            if (expect_success) {
                const auto& fs_ex = result.value();
                if (!fs_ex.is_populated) {
                     std::println(std::cout, "FAIL (is_populated is false)"); failures++; return;
                }

                struct stat expected_statbuf;
                int stat_ret = -1;
                std::filesystem::path path_for_stat_call = path_to_test;

                if (std::filesystem::is_symlink(std::filesystem::symlink_status(path_to_test)) && follow_symlinks_param) {
                    // If we followed a symlink, the comparison should be against the target's stat.
                    // Ensure `effective_symlink_target` is valid and exists for this comparison.
                    if (!effective_symlink_target.empty() && std::filesystem::exists(effective_symlink_target)) {
                        path_for_stat_call = effective_symlink_target;
                        stat_ret = ::stat(path_for_stat_call.c_str(), &expected_statbuf);
                    } else if (effective_symlink_target.empty() || !std::filesystem::exists(effective_symlink_target)) {
                        // This is a followed dangling symlink. get_full_status should have failed.
                        // This path in test logic means get_full_status succeeded, which is an error.
                        std::println(std::cout, "FAIL (get_full_status succeeded on a followed dangling/invalid symlink)");
                        failures++; return;
                    }
                } else {
                    // Regular file/dir, or a symlink itself (follow_symlinks_param is false)
                    stat_ret = ::lstat(path_to_test.c_str(), &expected_statbuf);
                }

                if (stat_ret != 0) {
                    std::println(std::cout, "FAIL (could not ::stat/lstat test entity '{}' for verification: {})", path_for_stat_call.string(), strerror(errno));
                    failures++;
                } else if (!compare_status(fs_ex, expected_statbuf, path_to_test)) {
                    // compare_status prints detailed mismatches
                    std::println(std::cout, "FAIL (status data mismatch)");
                    failures++;
                } else {
                    std::println(std::cout, "PASS");
                }

            } else { // Expected error, but got success
                std::println(std::cout, "FAIL (expected error, got success)");
                failures++;
            }
        } else { // Error case from get_full_status
            if (expect_success) {
                std::println(std::cout, "FAIL (expected success, got error: {})", result.error().message());
                failures++;
            } else { // Expected error and got an error
                bool error_code_matched = false;
                if (expected_ec_val_on_error) {
                    if (result.error() == *expected_ec_val_on_error) {
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

    // Global entities for symlink targets to manage their lifetime across tests
    TempTestEntity symlink_target_file_global("global_link_target_file", TempTestEntity::EntityType::File);
    TempTestEntity symlink_target_dir_global("global_link_target_dir", TempTestEntity::EntityType::Directory);
    std::filesystem::path non_existent_global_target = std::filesystem::temp_directory_path() / "global_non_existent_target";
    std::filesystem::remove(non_existent_global_target); // Ensure it's not there


    std::vector<StatusTestCase> test_cases = {
        {"File_Follow", TempTestEntity::EntityType::File, "", true, true, {}},
        {"File_NoFollow", TempTestEntity::EntityType::File, "", false, true, {}},
        {"Directory_Follow", TempTestEntity::EntityType::Directory, "", true, true, {}},
        {"Directory_NoFollow", TempTestEntity::EntityType::Directory, "", false, true, {}},

        {"SymlinkToFile_Follow", TempTestEntity::EntityType::Symlink, symlink_target_file_global.path, true, true, {}},
        {"SymlinkToFile_NoFollow", TempTestEntity::EntityType::Symlink, symlink_target_file_global.path, false, true, {}},
        {"SymlinkToDir_Follow", TempTestEntity::EntityType::Symlink, symlink_target_dir_global.path, true, true, {}},
        {"SymlinkToDir_NoFollow", TempTestEntity::EntityType::Symlink, symlink_target_dir_global.path, false, true, {}},

        {"DanglingSymlink_Follow", TempTestEntity::EntityType::Symlink, non_existent_global_target, true, false, std::errc::no_such_file_or_directory},
        {"DanglingSymlink_NoFollow", TempTestEntity::EntityType::Symlink, non_existent_global_target, false, true, {}},

        {"NonExistentFile", TempTestEntity::EntityType::File , "", true, false, std::errc::no_such_file_or_directory},
    };

    for (const auto& tc : test_cases) {
        // Re-assign effective_symlink_target for tests that use it.
        StatusTestCase current_tc = tc; // Copy to modify symlink target if needed for this run
        if (tc.entity_type_to_create == TempTestEntity::EntityType::Symlink) {
            if (tc.symlink_target_path_config.string().find("global_link_target_file") != std::string::npos) {
                current_tc.symlink_target_path_config = symlink_target_file_global.path;
            } else if (tc.symlink_target_path_config.string().find("global_link_target_dir") != std::string::npos) {
                current_tc.symlink_target_path_config = symlink_target_dir_global.path;
            } else if (tc.symlink_target_path_config.string().find("global_non_existent_target") != std::string::npos) {
                 current_tc.symlink_target_path_config = non_existent_global_target;
            }
             // If it's already an absolute path from TempTestEntity, it's fine.
        }
        current_tc.run(fs_ops, failures);
    }

    // Test specific fields for a regular file
    std::print(std::cout, "Test Case: RegularFile_FieldCheck... ");
    TempTestEntity check_file("field_check.txt", TempTestEntity::EntityType::File);

    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Small delay for timestamp granularity
    { std::ofstream ofs(check_file.path, std::ios::app); ofs << "more data"; } // "hello test" (10) + "more data" (9) = 19
     std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Ensure mtime is definitely after atime/ctime if possible

    auto res_check = fs_ops.get_full_status(check_file.path, true);
    if (res_check) {
        const auto& s = res_check.value();
        bool pass = true;
        struct stat expected_s_check;
        if (::stat(check_file.path.c_str(), &expected_s_check) != 0) {
             std::println(std::cout, "FAIL (stat failed for field check validation)"); pass = false; failures++;
        } else {
            if (s.type != std::filesystem::file_type::regular) { std::println(std::cerr, "\n  FieldCheck: type wrong"); pass = false;}
            if (s.file_size != static_cast<std::uintmax_t>(expected_s_check.st_size)) { std::println(std::cerr, "\n  FieldCheck: size wrong (expected {}, got {})", expected_s_check.st_size, s.file_size); pass = false; }
            if (s.uid != ::getuid()) { std::println(std::cerr, "\n  FieldCheck: uid wrong"); pass = false;}
            if (s.gid != ::getgid()) { std::println(std::cerr, "\n  FieldCheck: gid wrong"); pass = false;}
            if (std::abs(std::chrono::system_clock::to_time_t(s.mtime) - expected_s_check.st_mtime) > 2) { std::println(std::cerr, "\n  FieldCheck: mtime mismatch"); pass = false; }
            if (std::abs(std::chrono::system_clock::to_time_t(s.atime) - expected_s_check.st_atime) > 2) { std::println(std::cerr, "\n  FieldCheck: atime mismatch"); pass = false; }
            if (std::abs(std::chrono::system_clock::to_time_t(s.ctime) - expected_s_check.st_ctime) > 2) { std::println(std::cerr, "\n  FieldCheck: ctime mismatch"); pass = false; }
        }
        if (pass) std::println(std::cout, "PASS"); else {std::println(std::cout, "FAIL"); failures++;}
    } else {
        std::println(std::cout, "FAIL (get_full_status failed: {})", res_check.error().message());
        failures++;
    }

    if (failures > 0) {
        std::println(std::cerr, "\n{} XINIM::FS::GET_FULL_STATUS TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::GET_FULL_STATUS TESTS PASSED.");
    return EXIT_SUCCESS;
}
