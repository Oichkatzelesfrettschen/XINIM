// commands/tests/test_xinim_fs_chown_ops.cpp
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
#include <thread>       // For std::this_thread::sleep_for (not strictly needed for chown tests but good for consistency)


// POSIX includes
#include <unistd.h>     // For ::getuid, ::getgid, ::chown, ::lchown, ::symlink
#include <sys/types.h>  // For uid_t, gid_t
#include <sys/stat.h>   // For struct stat (used by get_status internally)
#include <pwd.h>        // For getpwuid if needed to find another user
#include <grp.h>        // For getgrgid if needed to find another group
#include <ctime>        // For time for srand
#include <cstring>      // For strerror


namespace { // Anonymous namespace for test helpers

class TempTestEntity {
public:
    std::filesystem::path path;
    enum class EntityType { File, Directory, Symlink };

    TempTestEntity(const std::string& name_prefix, EntityType type, const std::filesystem::path& symlink_target = "") {
        auto now_chrono = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now_chrono.time_since_epoch()).count();
        static int counter = 0;
        std::string unique_suffix = "_" + std::to_string(nanos) + "_" + std::to_string(counter++);
        path = std::filesystem::temp_directory_path() / (name_prefix + unique_suffix);

        std::error_code ec_setup;
        if (type == EntityType::Directory) {
            std::filesystem::create_directory(path, ec_setup);
        } else if (type == EntityType::File) {
            std::ofstream outfile(path);
            if (outfile) {
                outfile << "chown_test_content";
            } else {
                ec_setup = std::make_error_code(std::errc::io_error);
            }
        } else if (type == EntityType::Symlink) {
            if (symlink_target.empty()) {
                 std::println(std::cerr, "FATAL: Symlink target must be provided for TempTestEntity symlink type in test: {}", name_prefix);
                 std::exit(EXIT_FAILURE);
            }
            // Ensure target for symlink exists before creating symlink if it's part of this setup
            // For chown tests, the target's existence is important.
            if (!std::filesystem::exists(symlink_target) && !symlink_target.is_absolute()) { // Simple check
                 // For this test, assume symlink_target is managed outside or is absolute
            }
            std::filesystem::create_symlink(symlink_target, path, ec_setup);
        }
        if (ec_setup) {
             std::println(std::cerr, "FATAL: Test setup failed to create temporary entity '{}' (type {}): {}", path.string(), static_cast<int>(type), ec_setup.message());
             std::exit(EXIT_FAILURE);
        }
    }
    ~TempTestEntity() {
        std::error_code ec;
        // Use symlink_status for exists check as path could be a dangling symlink after test
        if (std::filesystem::exists(std::filesystem::symlink_status(path, ec))) {
            std::filesystem::remove_all(path, ec);
        }
        if (ec && ec != std::errc::no_such_file_or_directory) {
            std::println(std::cerr, "Warning: Failed to remove temp entity {}: {}", path.string(), ec.message());
        }
    }
    TempTestEntity(const TempTestEntity&) = delete;
    TempTestEntity& operator=(const TempTestEntity&) = delete;
};

// Helper to get another valid UID/GID for testing (if possible, not root)
// Returns current UID/GID if it cannot find a suitable 'other'.
// This is a very basic placeholder. Robust testing needs pre-configured users/groups.
uid_t get_other_uid(uid_t current_uid) {
    if (current_uid == 0) { // If current is root, try 'nobody' or uid 1
        struct passwd *pw = getpwnam("nobody");
        if (pw) return pw->pw_uid;
        return 1; // Common non-root uid
    }
    return 0; // If current is not root, try root for change
}
gid_t get_other_gid(gid_t current_gid) {
    if (current_gid == 0) { // If current is root group (wheel/root)
        struct group *gr = getgrnam("nogroup"); // common "nogroup"
        if (gr) return gr->gr_gid;
        gr = getgrnam("nobody"); // "nobody" group
        if (gr) return gr->gr_gid;
        return 1; // Common non-root gid
    }
    return 0; // If current is not root group, try root group
}

} // anonymous namespace

struct ChownTestCase {
    std::string name;
    TempTestEntity::EntityType entity_type;
    std::optional<uid_t> new_uid_spec;
    std::optional<gid_t> new_gid_spec;
    xinim::fs::operation_context ctx_params;

    bool expect_success;
    std::optional<std::errc> expected_ec_on_error;
    std::filesystem::path symlink_target_path_for_setup; // if entity_type is Symlink

    void run(int& failures) const {
        std::print(std::cout, "Test Case: {} (Mode: {}, Follow: {})... ", name,
                   ctx_params.execution_mode == xinim::fs::mode::standard ? "standard" : "direct",
                   ctx_params.follow_symlinks);

        TempTestEntity entity(name, entity_type, symlink_target_path_for_setup);

        xinim::fs::operation_context get_status_ctx = ctx_params; // Base context for get_status
        // When getting initial status to determine current UID/GID, we usually care about the item itself.
        // If entity is a symlink and we want to change the link's ownership (follow_symlinks=false for chown),
        // then initial UID/GID should be from the link.
        get_status_ctx.follow_symlinks = false; // Always lstat for initial properties of the path entry.

        auto initial_status_res = xinim::fs::get_status(entity.path, get_status_ctx);
        uid_t current_uid_of_entity = ::getuid(); // Fallback if stat fails
        gid_t current_gid_of_entity = ::getgid();

        if (initial_status_res) {
            current_uid_of_entity = initial_status_res.value().uid;
            current_gid_of_entity = initial_status_res.value().gid;
        } else if (name.find("NonExistent") == std::string::npos) { // If not a "non-existent" test, initial stat failure is a setup problem
             std::println(std::cout, "FAIL (setup: initial get_status failed on {} with {})", entity.path.string(), initial_status_res.error().message());
             failures++;
             return;
        }

        uid_t uid_to_set = new_uid_spec.value_or(current_uid_of_entity);
        gid_t gid_to_set = new_gid_spec.value_or(current_gid_of_entity);

        if (name.find("NonExistent") != std::string::npos) { // For non-existent file test
             uid_to_set = new_uid_spec.value_or(::getuid()); // Use some valid current uid/gid for the call
             gid_to_set = new_gid_spec.value_or(::getgid());
             std::filesystem::remove(entity.path); // Ensure it's gone
        }

        auto result = xinim::fs::change_ownership(entity.path, uid_to_set, gid_to_set, ctx_params);
        bool actual_success = result.has_value();
        std::error_code actual_ec = result ? std::error_code{} : result.error();

        if (actual_success) {
            if (expect_success) {
                xinim::fs::operation_context verify_ctx = ctx_params; // Use same follow policy for verification stat
                auto final_status_res = xinim::fs::get_status(entity.path, verify_ctx);

                if (!final_status_res) {
                    std::println(std::cout, "FAIL (verification: get_status failed after chown: {})", final_status_res.error().message()); failures++;
                } else {
                    bool uid_ok = (final_status_res.value().uid == uid_to_set);
                    bool gid_ok = (final_status_res.value().gid == gid_to_set);

                    if (uid_ok && gid_ok) {
                        std::println(std::cout, "PASS");
                    } else {
                        std::println(std::cout, "FAIL (Verification ownership mismatch. Got UID: {}, GID: {})", final_status_res.value().uid, final_status_res.value().gid);
                        if(!uid_ok) std::println(std::cerr, "  Expected UID: {}", uid_to_set);
                        if(!gid_ok) std::println(std::cerr, "  Expected GID: {}", gid_to_set);
                        failures++;
                    }
                }
            } else { std::println(std::cout, "FAIL (expected error, got success)"); failures++; }
        } else { /* Error case */
            if (expect_success) { std::println(std::cout, "FAIL (expected success, got error: {})", actual_ec.message()); failures++;}
            else {
                if (expected_ec_on_error && actual_ec == *expected_ec_on_error) { std::println(std::cout, "PASS (got expected error: {})", actual_ec.message()); }
                else if (!expected_ec_on_error && actual_ec) { std::println(std::cout, "PASS (any error was expected: {})", actual_ec.message());}
                else { std::println(std::cout, "FAIL (Error mismatch. Expected: {}, Got: {} ({}))",
                    expected_ec_on_error ? std::make_error_code(*expected_ec_on_error).message() : "any error",
                    actual_ec.value(), actual_ec.message()); failures++;}
            }
        }
    }
};


int main() {
    int failures = 0;
    srand(static_cast<unsigned int>(time(nullptr)));

    uid_t current_uid = ::getuid();
    gid_t current_gid = ::getgid();
    uid_t test_other_uid = get_other_uid(current_uid);
    gid_t test_other_gid = get_other_gid(current_gid);

    bool can_change_to_other = (current_uid == 0); // Only root can arbitrarily change ownership

    TempTestEntity base_test_dir("ChownTestBase", TempTestEntity::EntityType::Directory);
    TempTestEntity symlink_target_file_global(base_test_dir.path.string() + "/global_sym_target.txt", TempTestEntity::EntityType::File);

    std::vector<ChownTestCase> test_cases = {
        // Standard mode (should fail as std::filesystem doesn't support chown)
        {"StdMode_File_Fails", TempTestEntity::EntityType::File, current_uid, current_gid, {xinim::fs::mode::standard, false, true}, false, std::errc::operation_not_supported},

        // Direct mode - basic file and dir, no actual change if run as non-root
        {"DirectMode_File_CurrentUIDGID", TempTestEntity::EntityType::File, current_uid, current_gid, {xinim::fs::mode::direct, false, true}, true, {}},
        {"DirectMode_Dir_CurrentUIDGID", TempTestEntity::EntityType::Directory, current_uid, current_gid, {xinim::fs::mode::direct, false, true}, true, {}},

        // Direct mode - attempt to change UID (expected success if root, EPERM if not)
        {"DirectMode_File_OtherUID", TempTestEntity::EntityType::File, test_other_uid, std::nullopt, {xinim::fs::mode::direct, false, true}, can_change_to_other, can_change_to_other ? std::nullopt : std::optional(std::errc::permission_denied)},
        // Direct mode - attempt to change GID (expected success if root/member, EPERM if not)
        {"DirectMode_File_OtherGID", TempTestEntity::EntityType::File, std::nullopt, test_other_gid, {xinim::fs::mode::direct, false, true}, can_change_to_other, can_change_to_other ? std::nullopt : std::optional(std::errc::permission_denied)},
        {"DirectMode_File_OtherUIDGID", TempTestEntity::EntityType::File, test_other_uid, test_other_gid, {xinim::fs::mode::direct, false, true}, can_change_to_other, can_change_to_other ? std::nullopt : std::optional(std::errc::permission_denied)},

        // Non-existent file
        {"DirectMode_NonExistentFile_Fails", TempTestEntity::EntityType::File, current_uid, current_gid, {xinim::fs::mode::direct, false, true}, false, std::errc::no_such_file_or_directory},

        // Symlink tests (target for symlink_target_file_global.path)
        {"DirectMode_Symlink_Follow", TempTestEntity::EntityType::Symlink, current_uid, current_gid, {xinim::fs::mode::direct, false, true}, true, {}, symlink_target_file_global.path},
        {"DirectMode_Symlink_NoFollow", TempTestEntity::EntityType::Symlink, current_uid, current_gid, {xinim::fs::mode::direct, false, false}, true, {}, symlink_target_file_global.path},
        // Test changing ownership of symlink target via follow=true
        {"DirectMode_Symlink_Follow_ChangeTarget", TempTestEntity::EntityType::Symlink, test_other_uid, test_other_gid, {xinim::fs::mode::direct, false, true}, can_change_to_other, can_change_to_other ? std::nullopt : std::optional(std::errc::permission_denied), symlink_target_file_global.path},
    };

    for (auto tc : test_cases) { // Iterate by value to modify expect_success for EPERM cases
        if (tc.name == "DirectMode_NonExistentFile_Fails") {
            TempTestEntity non_existent_entity(tc.name + "_ne_setup", tc.entity_type, {}, false); // Don't auto-create
             // Run test with this non-existent path
            std::print(std::cout, "Test Case: {} (Mode: direct, Follow: {})... ", tc.name, tc.ctx_params.follow_symlinks);
            auto result_ne = xinim::fs::change_ownership(non_existent_entity.path, tc.new_uid_spec.value_or(current_uid), tc.new_gid_spec.value_or(current_gid), tc.ctx_params);
            if (!result_ne && result_ne.error() == std::errc::no_such_file_or_directory) {
                 std::println(std::cout, "PASS (got expected error: {})", result_ne.error().message());
            } else {
                 std::println(std::cout, "FAIL (non-existent error mismatch. Success: {}, Error: {})", result_ne.has_value(), result_ne.has_value() ? "" : result_ne.error().message()); failures++;
            }
            continue;
        }

        // Adjust EPERM expectations based on current UID for tests trying to change to "other"
        if ((tc.name.find("OtherUID") != std::string::npos || tc.name.find("OtherGID") != std::string::npos || tc.name.find("ChangeTarget") != std::string::npos)
            && current_uid != 0) {
            tc.expect_success = false;
            tc.expected_ec_on_error = std::errc::permission_denied;
        }

        tc.run(failures);
    }

    if (failures > 0) {
        std::println(std::cerr, "\n{} XINIM::FS::CHANGE_OWNERSHIP TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::CHANGE_OWNERSHIP TESTS PASSED.");
    return EXIT_SUCCESS;
}
